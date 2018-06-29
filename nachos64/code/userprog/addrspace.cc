// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace( AddrSpace* other)
{

	initData = other->initData;
	noInitData = other->noInitData;
	stack = other->stack;

	numPages = other->numPages;
	pageTable = new TranslationEntry[ numPages ];
	filename = other->filename;
	// iterar menos las 8 paginas de pila
	long dataAndCodePages = numPages - 8;
	long index;
	for (index = 0; index < dataAndCodePages; ++ index )
	{
		pageTable[index].virtualPage =  other->pageTable[index].virtualPage;
		pageTable[index].physicalPage = other->pageTable[index].physicalPage;
		pageTable[index].valid = other->pageTable[index].valid;
		pageTable[index].use = other->pageTable[index].use;
		pageTable[index].dirty = other->pageTable[index].dirty;
		pageTable[index].readOnly = other->pageTable[index].readOnly;
	}
	// 8 paginas para la pila
	/*
	printf("%s\n", "\nUsed pages: " );
	MemBitMap->Print();
	printf("\n");
	*/
	for (index = dataAndCodePages; index < numPages ; ++ index )
	{
		pageTable[index].virtualPage =  index;	// for now, virtual page # = phys page #
		pageTable[index].physicalPage = MemBitMap->Find();
		pageTable[index].valid = true;
		pageTable[index].use = false;
		pageTable[index].dirty = false;
		pageTable[index].readOnly = false;
	}
}

AddrSpace::AddrSpace(OpenFile *executable, std::string fn )
{

	NoffHeader noffH;
	unsigned int i, size;
	this->filename = fn;

	executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
	if ((noffH.noffMagic != NOFFMAGIC) &&
	(WordToHost(noffH.noffMagic) == NOFFMAGIC))
	SwapHeader(&noffH);
	ASSERT(noffH.noffMagic == NOFFMAGIC);

	// how big is address space?
	size = noffH.code.size + noffH.initData.size + noffH.uninitData.size
	+ UserStackSize;	// we need to increase the size
	// to leave room for the stack
	numPages = divRoundUp(size, PageSize);
	size = numPages * PageSize;

	//ASSERT(numPages <= NumPhysPages);		// check we're not trying
	// to run anything too big --
	// at least until we have
	// virtual memory

	DEBUG('a', "Initializing address space, num pages %d, size %d\n",
	numPages, size);
	// first, set up the translation
	pageTable = new TranslationEntry[numPages];
	for (i = 0; i < numPages; i++) {
		pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
		#ifdef USE_TLB
			pageTable[i].physicalPage = -1;
			pageTable[i].valid = false;
		#else
			pageTable[i].physicalPage = MemBitMap->Find();
			pageTable[i].valid = true;
		#endif
		pageTable[i].use = false;
		pageTable[i].dirty = false;
		pageTable[i].readOnly = false;  // if the code segment was entirely on
		// a separate page, we could set its
		// pages to be read-only
	}


	initData = divRoundUp(noffH.code.size, PageSize);
	noInitData = divRoundUp(noffH.initData.size, PageSize);
	stack = numPages - 8;
	// ajustes
	initData += noInitData;
	noInitData += stack;


	#ifndef VM
		// zero out the entire address space, to zero the unitialized data segment
		// and the stack segment
		//bzero(machine->mainMemory, size);

		// then, copy in the code and data segments into memory

		/* Para el segmento de codigo*/
		int x = noffH.code.inFileAddr;
		int y = noffH.initData.inFileAddr;
		int index;
		int codeNumPages = divRoundUp(noffH.code.size, numPages);
		int segmentNumPages = divRoundUp(noffH.initData.size, numPages);

		DEBUG('a', "Initializing code segment, at 0x%x, size %d, numero de paginas %d\n",
		noffH.code.virtualAddr, noffH.code.size, codeNumPages);

		for (index = 0; index < codeNumPages; ++ index )
		{
			executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *128 ] ),
			PageSize, x );
			x+=128;
		}

		if (noffH.initData.size > 0) {
			DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
			noffH.initData.virtualAddr, noffH.initData.size);
			for (index = codeNumPages; index < segmentNumPages; ++ index )
			{
				executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *128 ] ),
				PageSize, y );
				y+=128;
			}
		}
	#endif
	// then, copy in the code and data segments into memory
	/*
	if (noffH.code.size > 0) {
	DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
	noffH.code.virtualAddr, noffH.code.size);
	executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
	noffH.code.size, noffH.code.inFileAddr);
}


if (noffH.initData.size > 0) {
DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
noffH.initData.virtualAddr, noffH.initData.size);
executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
noffH.initData.size, noffH.initData.inFileAddr);
}
*/
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
	delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
	int i;

	for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

	// Initial program counter -- must be location of "Start"
	machine->WriteRegister(PCReg, 0);

	// Need to also tell MIPS where next instruction is, because
	// of branch delay possibility
	machine->WriteRegister(NextPCReg, 4);

	// Set the stack register to the end of the address space, where we
	// allocated the stack; but subtract off a bit, to make sure we don't
	// accidentally reference off the end!
	machine->WriteRegister(StackReg, numPages * PageSize - 16);
	DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState()
{}

	//----------------------------------------------------------------------
	// AddrSpace::RestoreState
	// 	On a context switch, restore the machine state so that
	//	this address space can run.
	//
	//      For now, tell the machine where to find the page table.
	//----------------------------------------------------------------------

	void AddrSpace::RestoreState()
	{
		#ifndef USE_TLB
			machine->pageTable = pageTable;
		#endif
		machine->pageTableSize = numPages;
	}

void AddrSpace::showTLBState()
{
		printf("TLB status\n");

		for (int index = 0; index < TLBSize; ++index )
		{
			printf("TLB[%d].paginaFisica = %d, paginaVirtual = %d, usada = %d\n",
			 	index, machine->tlb[ index ].physicalPage,
				machine->tlb[ index ].virtualPage, machine->tlb[ index ].use );
		}
		printf("\n");
}

int AddrSpace::writeIntoSwap( int physicalPageVictim )
{
	int swapAddress = SWAPBitMap->Find();
	OpenFile *executable = fileSystem->Open( SWAPFILENAME );
	if (executable == NULL) {
		 printf("Unable to open file %s\n", SWAPFILENAME );
		 ASSERT(false);
	}else
	{
		 printf("Prueba de escritura en el archivo %s\n", SWAPFILENAME );
		 executable->WriteAt((const char*)(&(machine->mainMemory[ ( physicalPageVictim * 128 ) ])),
		  	PageSize, swapAddress*PageSize );
		 delete executable;
	}
	return swapAddress;
}

void AddrSpace::readFromSwap( int swapPage, int physicalPage )
{
	OpenFile *executable = fileSystem->Open( SWAPFILENAME );
	if (executable == NULL) {
		 printf("Unable to open file %s\n", SWAPFILENAME );
		 ASSERT(false);
	}else
	{
		printf("Prueba de lectura en el archivo %s\n", SWAPFILENAME );
		executable->ReadAt( (&(machine->mainMemory[ ( physicalPage * 128 ) ])), PageSize, swapPage* PageSize );
		SWAPBitMap->Clear( swapPage );
		delete executable;
	}
}

/* for virtual mem*/
void AddrSpace::load(unsigned int vpn )
{
    //printf("Voy a necesitar la página : %d y  numero paginas %d\n", vpn, numPages );
    //if ( !machine->tlb[vpn].valid && !machine->tlb[vpn].dirty )
		int freeFrame;
		DEBUG('v', "Pagina que falla: %d\n", vpn);
		DEBUG('v', "\tCodigo va de [%d, %d[ \n", 0, initData- 1 );
		DEBUG('v',"\tDatos incializados va de [%d, %d[ \n", initData, noInitData);
		DEBUG('v', "\tDatos no incializados va de [%d, %d[ \n", initData ,stack );
		DEBUG('v',"\tPila va de [%d, %d[ \n", stack, numPages );
    if ( !pageTable[vpn].valid && !pageTable[vpn].dirty )
    {
				DEBUG('v',"\tQuiero una página invalida y limpia\n" );
	      /// debo leer el archivo
				DEBUG('v',"\tBusco en mi archivo fuente es: %s\n", filename.c_str() );
	      OpenFile* executable = fileSystem->Open( filename.c_str() );

	      if (executable == NULL) {
					 DEBUG('v',"Unable to open file %s\n", filename.c_str() );
	         ASSERT(false);
	      }

	      NoffHeader noffH;
	      executable->ReadAt((char *)&noffH, sizeof(noffH), 0);

	      /* pagina es de codigo */
	      if ( vpn >= 0 && vpn < initData )
	      {
	          /* leer del archivo */
	          freeFrame = MemBitMap->Find();

						if ( freeFrame != -1  )
						{
								DEBUG('v',"Frame libre en memoria: %d\n", freeFrame );
			          pageTable[ vpn ].physicalPage = freeFrame;
			          executable->ReadAt(&(machine->mainMemory[ ( freeFrame * 128 ) ] ),
			          PageSize, noffH.code.inFileAddr + PageSize*vpn );
			          pageTable[ vpn ].valid = true;

						}else
						{
							  DEBUG('v', "Buscar un espacio en el SWAP y elegir la victima para cambia\n" );
								ASSERT(false);
						}
						/* Se hace efectivo el cambio en la TLB*/
						machine->tlb[ indexFIFO ].virtualPage = vpn;
						machine->tlb[ indexFIFO ].physicalPage = freeFrame;
						machine->tlb[ indexFIFO ].valid = true;
						DEBUG('v',"Uso campo en TLB: %d\n", indexFIFO );
						++indexFIFO;
						indexFIFO = indexFIFO % TLBSize;
	      /* es de datos */
	    }else if( vpn >= initData && vpn < noInitData ) // puede que no haya inicializado como en halt :(
      {
						DEBUG('v',"Pagina de datos inicializados\n" );
            ASSERT(false);

      }else if( vpn >= noInitData && vpn < stack ) // puede que no haya inicializado como en halt :(
      {
						DEBUG('v', "Pagina de datos no inicializados\n" );
            ASSERT(false);
      }
      else if ( vpn >= stack && vpn < numPages  )
      {
				DEBUG('v',"\t\tPágina de pila\n");
				freeFrame = MemBitMap->Find();
				DEBUG('v',"\t\t\tSe busca una nueva página para otorgar\n" );
				if ( freeFrame != -1 )
				{
						DEBUG('v', "Se le otroga una nueva página en memoria\n" );
						pageTable[ vpn ].physicalPage = freeFrame;
						pageTable[ vpn ].valid = true;

						/* Se hace efectivo el cambio en la TLB*/
						machine->tlb[ indexFIFO ].virtualPage = vpn;
						machine->tlb[ indexFIFO ].physicalPage = freeFrame;
						machine->tlb[ indexFIFO ].valid = true;
						DEBUG('v',"Uso campo en TLB: %d\n", indexFIFO );
						++indexFIFO;
						indexFIFO = indexFIFO % TLBSize;

				}else
				{
					DEBUG('v', "\t\t\tUps! No hay más memoria, es momento de enviar a una victima al swap\n" );
					showTLBState();
					ASSERT(false);
				}
        //ASSERT(false);
      }else
      {
				DEBUG('v',"%sCodigo: %d, Datos: ini=%d y  no ini = %d, Pila: %d, Numero de pags: %d\n", "Algo raro ", 0,
        initData - noInitData , noInitData - stack -1 , stack, numPages );
        ASSERT(false);
      }
      delete executable;
    }
    //else if (!machine->tlb[vpn].valid && machine->tlb[vpn].dirty)
    else if ( !pageTable[vpn].valid && pageTable[vpn].dirty )
    {
			DEBUG('v', "\tLa cual es invalida y sucia\n" );
      ASSERT(false);

    }
    //else if ( machine->tlb[vpn].valid && !machine->tlb[vpn].dirty)
    else if ( pageTable[vpn].valid && !pageTable[vpn].dirty ) /* CASO 1 0*/
    {
			DEBUG('v',"Es válida y limpia\n");
			DEBUG('v', "\tPagina puede estar en swap o en memoria\n" );
			bool isPageInMem = MemBitMap->Test( pageTable[ vpn ].physicalPage );
			if ( isPageInMem )
			{
					DEBUG('v', "\t\tPagina está en memoria, se hace efectivo el cambio en el TBL\n" );
					/* Se hace efectivo el cambio en la TLB*/
					machine->tlb[ indexFIFO ].virtualPage = vpn;
					machine->tlb[ indexFIFO ].physicalPage = pageTable[ vpn ].physicalPage;
					machine->tlb[ indexFIFO ].valid = true;
					DEBUG('v',"Uso campo en TLB: %d\n", indexFIFO );
					++indexFIFO;
					indexFIFO = indexFIFO % TLBSize;

					//just for testing swap
					printf("\nPrueba de SWAP. Unicamente se mete la pagina y se vuelve a sacar a memoria\n");
					int dummy = writeIntoSwap( pageTable[ vpn ].physicalPage );
					readFromSwap( dummy, pageTable[ vpn ].physicalPage );
					printf("\n");
			}
			else
			{
					DEBUG('v', "\t\tPagina está en SWAP\n" );
					ASSERT(false);
			}
    }else
    {
			DEBUG('v', "\tLa cual es valida y sucia\n" );
      ASSERT(false);
    }
}
