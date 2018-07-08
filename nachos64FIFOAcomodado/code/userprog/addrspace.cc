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
		pageTable[index].virtualPage =  index;
		pageTable[index].physicalPage = other->pageTable[index].physicalPage;
		pageTable[index].valid = other->pageTable[index].valid;
		pageTable[index].use = other->pageTable[index].use;
		pageTable[index].dirty = other->pageTable[index].dirty;
		pageTable[index].readOnly = other->pageTable[index].readOnly;
	}
	// 8 paginas para la pila

	for (index = dataAndCodePages; index < numPages ; ++ index )
	{
		pageTable[index].virtualPage =  index;	// for now, virtual page # = phys page #
		#ifndef VM
		pageTable[index].physicalPage = MemBitMap->Find();
		pageTable[index].valid = true;
		#else
		pageTable[index].physicalPage = -1;
		pageTable[index].valid =false;
		#endif
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
		#ifdef VM
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
	noInitData = initData + divRoundUp(noffH.initData.size, PageSize);
	stack = numPages - divRoundUp(UserStackSize,PageSize);

	#ifndef VM
	printf("\n\n\n\t\t Virtual mem is no define\n\n\n");
	// zero out the entire address space, to zero the unitialized data segment
	// and the stack segment
	//bzero(machine->mainMemory, size);

	// then, copy in the code and data segments into memory

	/* Para el segmento de codigo*/
	int x = noffH.code.inFileAddr;
	int y = noffH.initData.inFileAddr;
	int index;
	int codeNumPages = divRoundUp(noffH.code.size, PageSize);
	int segmentNumPages = divRoundUp(noffH.initData.size, PageSize);

	DEBUG('a', "Initializing code segment, at 0x%x, size %d, numero de paginas %d\n",
	noffH.code.virtualAddr, noffH.code.size, codeNumPages);

	for (index = 0; index < codeNumPages; ++ index )
	{
		executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *PageSize ] ),
		PageSize, x );
		x+=PageSize;
	}

	if (noffH.initData.size > 0) {
		DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
		noffH.initData.virtualAddr, noffH.initData.size);
		for (index = codeNumPages; index < codeNumPages + segmentNumPages; ++ index )
		{
			executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *PageSize ] ),
			PageSize, y );
			y+=PageSize;
		}
	}
	#endif
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
{
	#ifdef VM
	DEBUG ( 't', "\nSe salva el estado del hilo: %s\n", currentThread->getName() );
	for(int i = 0; i < TLBSize; ++i){
		pageTable[machine->tlb[i].virtualPage].use = machine->tlb[i].use;
		pageTable[machine->tlb[i].virtualPage].dirty = machine->tlb[i].dirty;
	}
	/*
	if (machine->tlb != NULL)
	{
		delete [] machine->tlb;
		machine->tlb = NULL;
	}
	*/
	machine->tlb = new TranslationEntry[ TLBSize ];
	for (int i = 0; i < TLBSize; ++i)
	{
		machine->tlb[i].valid = false;
	}
	#endif
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState()
{
	DEBUG ( 't', "\nSe restaura el estado del hilo: %s\n", currentThread->getName() );
	#ifndef VM
	machine->pageTable = pageTable;
	machine->pageTableSize = numPages;
	#else
	indexTLBFIFO = 0;
	threadFirstTime = true;
	/*
	if (machine->tlb != NULL)
	{
		delete [] machine->tlb;
		machine->tlb = NULL;
	}
	*/
	machine->tlb = new TranslationEntry[ TLBSize ];
	for (int i = 0; i < TLBSize; ++i)
	{
		machine->tlb[i].valid = false;
	}
	#endif
}

void AddrSpace::clearPhysicalPage( int physicalPage )
{
	for (int index = 0; index < PageSize; ++index )
	{
		machine->mainMemory[ physicalPage*PageSize + index ] = 0;
	}
}

void AddrSpace::showIPTState()
{
	for (unsigned int x = 0; x < NumPhysPages; ++x)
	{
		if( IPT[x] != NULL)
		DEBUG('v',"PhysicalPage = %d, VirtualPage = %d, Valid = %d, Dirty = %d \n", IPT[x]->physicalPage, IPT[x]->virtualPage, IPT[x]->valid, IPT[x]->dirty );
	}
}

void AddrSpace::showPageTableState()
{
	for (unsigned int x = 0; x < numPages; ++x)
	{
		DEBUG('v',"Index [%d] .virtualPage = %d, .physicalPage = %d, .use = %d, .dirty = %d, valid = %d\n"
		,x,pageTable[x].virtualPage, pageTable[x].physicalPage, pageTable[x].use, pageTable[x].dirty, pageTable[x].valid );
	}
}

void AddrSpace::showTLBState()
{
	printf("TLB status\n");
	for (int index = 0; index < TLBSize; ++index )
	{
		printf("TLB[%d].paginaFisica = %d, paginaVirtual = %d, usada = %d, sucia = %d, valida = %d\n",
		index, machine->tlb[ index ].physicalPage,
		machine->tlb[ index ].virtualPage, machine->tlb[ index ].use, machine->tlb[ index ].dirty, machine->tlb[ index ].valid );
	}
	printf("\n");
}

int AddrSpace::updateTLBFIFO(unsigned int vpn)
{
	//Debo guardar los cambios hechos en la TLB a la pagina que estoy sacando.
	int returnIndex = 0;

	if ( threadFirstTime == false )
	{
		pageTable[machine->tlb[indexTLBFIFO].virtualPage].use = machine->tlb[indexTLBFIFO].use;
		pageTable[machine->tlb[indexTLBFIFO].virtualPage].dirty = machine->tlb[indexTLBFIFO].dirty;
	}else
	{
		threadFirstTime = false;
	}
	//Actualizo el TLB
	machine->tlb[indexTLBFIFO].virtualPage =  pageTable[vpn].virtualPage;
	machine->tlb[indexTLBFIFO].physicalPage = pageTable[vpn].physicalPage;
	machine->tlb[indexTLBFIFO].valid = pageTable[vpn].valid;
	machine->tlb[indexTLBFIFO].use = pageTable[vpn].use;
	machine->tlb[indexTLBFIFO].dirty = pageTable[vpn].dirty;
	machine->tlb[indexTLBFIFO].readOnly = pageTable[vpn].readOnly;
	DEBUG('v', "\tPagina del TLB asignada %d\n\n", (indexTLBFIFO)%TLBSize);
	returnIndex = indexTLBFIFO;
	indexTLBFIFO = (indexTLBFIFO + 1)%TLBSize;
	return returnIndex;
}

void AddrSpace::useVictimTLBSpace(int tlbIndex, int vpn )
{
	machine->tlb[tlbIndex].virtualPage =  pageTable[vpn].virtualPage;
	machine->tlb[tlbIndex].physicalPage = pageTable[vpn].physicalPage;
	machine->tlb[tlbIndex].valid = pageTable[vpn].valid;
	machine->tlb[tlbIndex].use = pageTable[vpn].use;
	machine->tlb[tlbIndex].dirty = pageTable[vpn].dirty;
	machine->tlb[tlbIndex].readOnly = pageTable[vpn].readOnly;
}


void AddrSpace::writeIntoSwap( int physicalPageVictim ){
	int swapPage = SWAPBitMap->Find();
	ASSERT( physicalPageVictim >= 0 );
	DEBUG('h', "\t\t\t\tSe escirbe en el swap en la posición: %d\n",swapPage );
	if ( swapPage == -1 )
	{
		printf("%s\n", "Error(writeIntoSwap): Espacio en SWAP NO disponible\n");
		ASSERT( false );
	}
	OpenFile *swapFile = fileSystem->Open( SWAPFILENAME );
	if( swapFile == NULL ){
		printf("%s\n", "Error(writeIntoSwap): No se pudo habir el archivo de SWAP");
		ASSERT(false);
	}
	IPT[physicalPageVictim]->valid = false;
	IPT[physicalPageVictim]->physicalPage = swapPage;
	swapFile->WriteAt((&machine->mainMemory[physicalPageVictim*PageSize]),PageSize, swapPage*PageSize);
	MemBitMap->Clear( indexSWAPFIFO );
	//clearPhysicalPage( indexSWAPFIFO );
	++stats->numDiskWrites;
	delete swapFile;
}

void AddrSpace::readFromSwap( int physicalPage , int swapPage ){
	DEBUG('h', "\t\t\t\tSe lee en el swap en la posición: %d\n",swapPage );
	//SWAPBitMap->Print();
	SWAPBitMap->Clear(swapPage);
	//SWAPBitMap->Print();
	OpenFile *swapFile = fileSystem->Open(SWAPFILENAME);
	ASSERT( swapPage >=0 && swapPage < SWAPSize );
	if(swapFile == NULL){
		printf("%s\n", "Error(writeIntoSwap): No se pudo habir el archivo de SWAP");
		ASSERT(false);
	}
	swapFile->ReadAt((&machine->mainMemory[physicalPage*PageSize]), PageSize, swapPage*PageSize);
	++stats->numPageFaults;
	++stats->numDiskReads;
	delete swapFile;
}

int  AddrSpace::findVictimInTLB( int indexSWAP )
{
	int tlbIndexForNewPage = -1;
	for ( int index = 0; index < TLBSize; ++index )
	{
		//ASSERT( machine->tlb[ index ].valid );
		if ( machine->tlb[ index ].valid && machine->tlb[ index ].physicalPage == IPT[indexSWAP]->physicalPage  )
		{
			DEBUG('v',"%s\n", "\t\t\tSí estaba la victima en TLB" );
			machine->tlb[ index ].valid = false;
			IPT[indexSWAP]->use = machine->tlb[ index ].use;
			IPT[indexSWAP]->dirty = machine->tlb[ index ].dirty;
			tlbIndexForNewPage = index;
			break;
		}
	}
	return tlbIndexForNewPage;
}

//VM
void AddrSpace::load(unsigned int vpn)
{
	int freeFrame;
	DEBUG('v', "Numero de paginas: %d, hilo actual: %s\n", numPages, currentThread->getName());
	DEBUG('v', "\tCodigo va de [%d, %d[ \n", 0, initData);
	DEBUG('v',"\tDatos incializados va de [%d, %d[ \n", initData, noInitData);
	DEBUG('v', "\tDatos no incializados va de [%d, %d[ \n", noInitData , stack);
	DEBUG('v',"\tPila va de [%d, %d[ \n", stack, numPages );

	//Si la pagina no es valida ni esta sucia.
	if ( !pageTable[vpn].valid && !pageTable[vpn].dirty ){
		//Entonces dependiendo del segmento de la pagina, debo tomar la decisión de ¿donde cargar esta pagina?
		DEBUG('v', "\t1-La pagina es invalida y limpia\n");
		DEBUG('v', "\tArchivo fuente: %s\n", filename.c_str());
		++stats->numPageFaults;
		OpenFile* executable = fileSystem->Open( filename.c_str() );
		if (executable == NULL) {
			DEBUG('v',"Unable to open source file %s\n", filename.c_str() );
			ASSERT(false);
		}
		NoffHeader noffH;
		executable->ReadAt((char *)&noffH, sizeof(noffH), 0);

		//Nesecito verificar a cual segemento pertenece la pagina.
		if(vpn >= 0 && vpn < initData){ //segemento de Codigo
			DEBUG('v', "\t1.1 Página de código\n");
			//Se debe cargar la pagina del archivo ejecutable.
			freeFrame = MemBitMap->Find();

			if ( freeFrame != -1  )
			{
				DEBUG('v',"\tFrame libre en memoria: %d\n", freeFrame );
				pageTable[ vpn ].physicalPage = freeFrame;
				executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
				PageSize, noffH.code.inFileAddr + PageSize*vpn );
				pageTable[ vpn ].valid = true;
				//pageTable[ vpn ].readOnly = true;

				//Se actualiza la TLB invertida
				IPT[freeFrame] = &(pageTable[ vpn ]);
				//Se debe actualizar el TLB
				updateTLBFIFO(vpn);
				//updateTLBSC(vpn);
			}else
			{		//victima swap
				int tlbIndexForNewPage = findVictimInTLB( indexSWAPFIFO );

				if ( IPT[indexSWAPFIFO]->dirty )
				{
					DEBUG('v',"\t\t\tVictima f=%d,l=%d y  sucia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					writeIntoSwap( IPT[indexSWAPFIFO]->physicalPage );

					// pedir el nuevo freeFrame
					freeFrame = MemBitMap->Find();
					// verificar que sea distinto de -1
					if ( -1 == freeFrame )
					{
						printf("Invalid free frame %d\n", freeFrame );
						ASSERT( false );
					}
					// actualizar la pagina física para la nueva virtual vpn
					pageTable[ vpn ].physicalPage = freeFrame;
					//  cargar el código a la memoria
					//++stats->numPageFaults;
					executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
					PageSize, noffH.code.inFileAddr + PageSize*vpn );
					// actualizar la validez
					pageTable[ vpn ].valid = true;
					// actualizar la tabla de paginas invertidas
					IPT[ freeFrame ] = &( pageTable[ vpn ] );

					// finalmente, actualizar tlb
					if ( tlbIndexForNewPage == -1 )
					{
						DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
						updateTLBFIFO( vpn );
					}else
					{
						DEBUG('v',"1.1 - Copio en el tlb de la victima a la nueva\n" );
					}
					//ASSERT(false);
				}else
				{
					DEBUG('v',"\t\t\tVictima f=%d,l=%d y limpia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
					IPT[indexSWAPFIFO]->valid = false;
					IPT[indexSWAPFIFO]->physicalPage = -1;
					MemBitMap->Clear( oldPhysicalPage );
					//clearPhysicalPage( oldPhysicalPage );

					// cargamos pargina nueva en memoria
					freeFrame = MemBitMap->Find();
					if ( freeFrame == -1 )
					{
						printf("Invalid free frame %d\n", freeFrame );
						ASSERT( false );
					}
					//++stats->numPageFaults;
					pageTable[ vpn ].physicalPage = freeFrame;
					executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
					PageSize, noffH.code.inFileAddr + PageSize*vpn );
					pageTable[ vpn ].valid = true;

					IPT[ freeFrame ] = &(pageTable [ vpn ]);
					if ( tlbIndexForNewPage == -1 )
					{
						DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
						updateTLBFIFO( vpn );
					}else
					{
						DEBUG('v',"Copio en el tlb de la victima la nueva\n" );
						useVictimTLBSpace( tlbIndexForNewPage, vpn );
					}
				}
				indexSWAPFIFO = ( indexSWAPFIFO +1 )%NumPhysPages;
				//ASSERT(false);
			}
		}
		else if(vpn >= initData && vpn < noInitData){ //segmento de Datos Inicializados.
			//Se debe cargar la pagina del archivo ejecutable.
			DEBUG('v', "\t1.2 Página de datos Inicializados\n");
			freeFrame = MemBitMap->Find();

			if ( freeFrame != -1  )
			{
				DEBUG('v',"Frame libre en memoria: %d\n", freeFrame );
				//++stats->numPageFaults;
				pageTable[ vpn ].physicalPage = freeFrame;
				executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
				PageSize, noffH.code.inFileAddr + PageSize*vpn );
				pageTable[ vpn ].valid = true;

				//Se actualiza la TLB invertida
				IPT[freeFrame] = &(pageTable[ vpn ]);
				//Se debe actualizar el TLB
				updateTLBFIFO(vpn);
				//updateTLBSC(vpn);
			}else
			{
				//Se debe selecionar una victima para enviar al SWAP
				// el método es muy similar al de código
				// revisar si la victima esta en el TLB
				int tlbIndexForNewPage = findVictimInTLB( indexSWAPFIFO );
				// revisar si la victima está sucia
				if ( IPT[indexSWAPFIFO]->dirty )
				{
					//si sí
						//envíarla al swap
						DEBUG('v',"\t\t\tVictima f=%d,l=%d y  sucia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
						writeIntoSwap( IPT[indexSWAPFIFO]->physicalPage );
						//pedir el nuevo freeFrame
						freeFrame = MemBitMap->Find();
						//validar ese nuevo freeFrame
						if ( -1 == freeFrame )
						{
							printf("Invalid free frame %d\n", freeFrame );
							ASSERT( false );
						}
						// asignar al pageTable[vpn] es freeFrame
						pageTable[ vpn ].physicalPage = freeFrame;
						// leer del archivo ejecutable
						executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
						PageSize, noffH.code.inFileAddr + PageSize*vpn );
						//poner valida la paginas
						pageTable[ vpn ].valid = true;
						//actualiza la tabla de paginas invertidas
						IPT[ freeFrame ] = &( pageTable[ vpn ] );
						//hacer la actualización en la tlb
						if ( tlbIndexForNewPage == -1 )
						{
							DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
							updateTLBFIFO( vpn );
						}else
						{
							DEBUG('v',"1.1 - Copio en el tlb de la victima a la nueva\n" );
						}
				}else
				{
					//si no esta sucia
					DEBUG('v',"\t\t\tVictima f=%d,l=%d y limpia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
						// rescatar la antigua fisica de la victima
						int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
						MemBitMap->Clear( oldPhysicalPage );
						// poner a la victima en  valid = false
						IPT[indexSWAPFIFO]->valid = false;
						//ponerle la pagina fisica en -1
						IPT[indexSWAPFIFO]->physicalPage = -1;
						// pedir el nuevo freeframe
						freeFrame = MemBitMap->Find();
						//validar ese nuevo freeFrame
						if ( freeFrame == -1 )
						{
							printf("Invalid free frame %d\n", freeFrame );
							ASSERT( false );
						}
						//asignar ese freeFrame al pageTable[vpn]
						pageTable[ vpn ].physicalPage = freeFrame;
						//leer del archivo ejecutable
						executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
						PageSize, noffH.code.inFileAddr + PageSize*vpn );
						//valida dicha pageTable[vpn]
						pageTable[ vpn ].valid = true;
						//actualizar tabla de paginas invertidas
						IPT[ freeFrame ] = &(pageTable [ vpn ]);
						// actualizar la tlp
						if ( tlbIndexForNewPage == -1 )
						{
							DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
							updateTLBFIFO( vpn );
						}else
						{
							DEBUG('v',"Copio en el tlb de la victima la nueva\n" );
							useVictimTLBSpace( tlbIndexForNewPage, vpn );
						}
				}
				// aumentar el índice fifo del swap
				indexSWAPFIFO = ( indexSWAPFIFO +1 )%NumPhysPages;
				//ASSERT(false);
			}
		}
		else if(vpn >= noInitData && vpn < numPages){ //segemento de Datos No Inicializados o segmento de Pila.
			DEBUG('v',"\t1.3 Página de datos no Inicializado o página de pila\n");
//			///++stats->numPageFaults;
			freeFrame = MemBitMap->Find();
			DEBUG('v',"\t\t\tSe busca una nueva página para otorgar\n" );
			if ( freeFrame != -1 )
			{
				//DEBUG('v', "Se le otorga una nueva página en memoria\n" );
				pageTable[ vpn ].physicalPage = freeFrame;
				pageTable[ vpn ].valid = true;

				//Se actualiza la TLB invertida
				//clearPhysicalPage(freeFrame);
				IPT[freeFrame] = &(pageTable[ vpn ]);

				//Se debe actualizar el TLB
				updateTLBFIFO(vpn);
				//updateTLBSC(vpn);

			}else{
				// usar swap
				int tlbIndexForNewPage = findVictimInTLB( indexSWAPFIFO );
				// revisamos con la información actualizada a la victima
				if ( IPT[indexSWAPFIFO]->dirty )
				{
					DEBUG('v',"\t\t\tVictima f=%d,l=%d sucia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					writeIntoSwap( IPT[indexSWAPFIFO]->physicalPage );

					// pedir el nuevo freeFrame
					freeFrame = MemBitMap->Find();

					// verificar que sea distinto de -1
					if ( -1 == freeFrame )
					{
						printf("Invalid free frame %d\n", freeFrame );
						ASSERT( false );
					}

					// actualizar la pagina física para la nueva virtual vpn
					pageTable [ vpn ].physicalPage = freeFrame;
					pageTable [ vpn ].valid = true;

					//actualizo invertida
					IPT[ freeFrame ] = &(pageTable [ vpn ]);

					//actualizo el tlb
					if ( tlbIndexForNewPage == -1 )
					{
						DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
						updateTLBFIFO( vpn );
					}else
					{
						DEBUG('v',"Copio en el tlb de la victima la nueva\n" );
						useVictimTLBSpace( tlbIndexForNewPage, vpn );
					}
					//ASSERT(false);
				}else
				{
					DEBUG('v',"\t\t\tVictima f=%d,l=%d limpia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
					IPT[indexSWAPFIFO]->valid = false;
					IPT[indexSWAPFIFO]->physicalPage = -1;
					MemBitMap->Clear( oldPhysicalPage );
					clearPhysicalPage( oldPhysicalPage );

					// cargamos pargina nueva en memoria
					freeFrame = MemBitMap->Find();

					if ( freeFrame == -1 )
					{
						DEBUG('v',"Invalid free frame %d\n", freeFrame );
						ASSERT( false );
					}
					pageTable [ vpn ].physicalPage = freeFrame;
					pageTable [ vpn ].valid = true;

					IPT[ freeFrame ] = &(pageTable [ vpn ]);

					if ( tlbIndexForNewPage == -1 )
					{
						DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
						updateTLBFIFO( vpn );
					}else
					{
						DEBUG('v',"Copio en el tlb de la victima la nueva\n" );
						useVictimTLBSpace( tlbIndexForNewPage, vpn  );
					}
				}
				indexSWAPFIFO = ( indexSWAPFIFO +1 )%NumPhysPages;
				//ASSERT(false);
			}
		}
		else{
			printf("%s %d\n", "Algo muy malo paso, el numero de pagina invalido!", vpn);
			ASSERT(false);
		}
		// Se cierra el Archivo
		delete executable;
	}
	//Si la pagina no es valida y esta sucia.
	else if(!pageTable[vpn].valid && pageTable[vpn].dirty){
		//Debo traer la pagina del area de SWAP.
		DEBUG('v', "\t2- Pagina invalida y sucia\n");
		DEBUG('v', "\t\tPagina física: %d, pagina virtual= %d\n", pageTable[vpn].physicalPage, pageTable[vpn].virtualPage );
		freeFrame = MemBitMap->Find();
		if(freeFrame != -1)
		{ //Si hay especio en memoria
			DEBUG('v',"%s\n", "Si hay espacio en memoria, solo leemos de SWAP\n" );

			//actualizar su pagina física de la que leo del swap
			int oldSwapPageAddr = pageTable [ vpn ].physicalPage;
			pageTable [ vpn ].physicalPage = freeFrame;
			// cargarla
			readFromSwap( freeFrame, oldSwapPageAddr );
			// actualizar tambien su validez
			pageTable [ vpn ].valid = true;
			// actualiza tabla de paginas invertidas
			IPT[ freeFrame ] = &(pageTable [ vpn ]);
			//actualizar su posición en la tlb
			updateTLBFIFO( vpn );
			//ASSERT(false);
		}
		else
		{
			//Se debe selecionar una victima para enviar al SWAP
			DEBUG('v',"\n%s\n", "NO hay memoria, es más complejo.\n" );

			int tlbIndexForNewPage = findVictimInTLB( indexSWAPFIFO );
			if ( IPT[indexSWAPFIFO]->dirty )
			{
				DEBUG('v',"\t\t\tVictima f=%d,l=%d sucia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
				writeIntoSwap( IPT[indexSWAPFIFO]->physicalPage );
				// pido el freeFrame
				freeFrame = MemBitMap->Find();
				if ( freeFrame == -1 )
				{
					printf("Invalid free frame %d\n", freeFrame );
					ASSERT( false );
				}

				int oldSwapPageAddr = pageTable [ vpn ].physicalPage;
				pageTable [ vpn ].physicalPage = freeFrame;

				// cargamos la pagina que desea desde el SWAP
				readFromSwap( freeFrame, oldSwapPageAddr );
				pageTable [ vpn ].valid = true;

				IPT[ freeFrame ] = &(pageTable [ vpn ]);

				// finalmente actualizacom tlb
				if ( tlbIndexForNewPage == -1 )
				{
					DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
					updateTLBFIFO( vpn );
				}else
				{
					DEBUG('v',"2-Copio en el tlb de la victima la nueva\n" );
					useVictimTLBSpace( tlbIndexForNewPage, vpn );
				}
				//ASSERT(false);
			}else
			{
				DEBUG('v',"\t\t\tVictima f=%d,l=%d limpia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
				int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
				IPT[indexSWAPFIFO]->valid = false;
				IPT[indexSWAPFIFO]->physicalPage = -1;
				MemBitMap->Clear( oldPhysicalPage );
				//clearPhysicalPage( oldPhysicalPage );

				freeFrame = MemBitMap->Find();

				if ( freeFrame == -1 )
				{
					printf("Invalid free frame %d\n", freeFrame );
					ASSERT( false );
				}
				int oldSwapPageAddr = pageTable [ vpn ].physicalPage;
				pageTable [ vpn ].physicalPage = freeFrame;

				// cargamos la pagina que desea desde el SWAP
				readFromSwap( freeFrame, oldSwapPageAddr );

				pageTable [ vpn ].valid = true;

				IPT[ freeFrame ] = &(pageTable [ vpn ]);

				// finalmente actualizacom tlb
				if ( tlbIndexForNewPage == -1 )
				{
					DEBUG('v',"NO COPIO en el tlb de la victima la nueva\n" );
					updateTLBFIFO( vpn );
				}else
				{
					DEBUG('v',"2-Copio en el tlb de la victima la nueva\n" );
					useVictimTLBSpace( tlbIndexForNewPage, vpn );
				}
			}
			indexSWAPFIFO = ( indexSWAPFIFO +1 )%NumPhysPages;
		}
		//ASSERT(false);
	}
	//Si la pagina es valida y no esta sucia.
	else if(pageTable[vpn].valid && !pageTable[vpn].dirty){
		DEBUG('v', "\t3- Pagina valida y limpia\n");
		//La pagina ya esta en memoria por lo que solamente debo actualizar el TLB.
		updateTLBFIFO(vpn);
		//++stats->numPageFaults;
	}
	//Si la pagina es valida y esta sucia.
	else{
		DEBUG('v', "\t4- Pagina valida y sucia\n");
		//La pagina ya esta en memoria por lo que solamente debo actualizar el TLB.
		updateTLBFIFO(vpn);
		//++stats->numPageFaults;
		//ASSERT(false);
	}
}
