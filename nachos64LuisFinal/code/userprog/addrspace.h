// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include <string>

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
public:
  AddrSpace(AddrSpace* other);
  AddrSpace(OpenFile *executable, std::string filename = "NULL" );
  // Create an address space,
  // initializing it with the program
  // stored in the file "executable"
  ~AddrSpace();			// De-allocate an address space

  void InitRegisters();		// Initialize user-level CPU registers,
  // before jumping to user code

  void SaveState();			// Save/restore address space-specific
  void RestoreState();		// info on a context switch

public:
  static const int code = 0;
  unsigned int data;
  unsigned int initData;
  unsigned int noInitData;
  unsigned int stack;
  std::string filename;

  unsigned int numPages;		// Number of pages in the virtual

private:
  TranslationEntry *pageTable;	// Assume linear page table translation
  void showTLBState();
  void showIPTState();
  void showPageTableState();
  void writeIntoSwap( int physicalPageVictim );
  void readFromSwap( int physicalPage , int swapPage);
  int updateTLBSC(unsigned int vpn);
  int updateTLBFIFO(unsigned int vpn);
  void validateSWAPCaseFIFO();
  void takeFromSWAPToMem( unsigned int vpn );
  void clearPhysicalPage( int physicalPage );
  void useVictimTLBSpace(int tlbIndex, int vpn );
  int  findVictimInTLB( int indexSWAP );
  int  findVictimInMemFIFO();
  int  findVictimInMemSC();

  // for now!
  // address space
public:
  void load(unsigned int vpn );

};

#endif // ADDRSPACE_H
