/*
 * xmem.cpp
 *
 *  Created on: 21 Aug 2011
 *      Author: Andy Brown
 *     Website: www.andybrown.me.uk
 *
 *  This work is licensed under a Creative Commons Attribution-ShareAlike 3.0 Unported License.
 */


#ifndef __89089DA1_BAAC_497C_8E1FFEF0911A6844
#define __89089DA1_BAAC_497C_8E1FFEF0911A6844

#include <avr/io.h>

#if defined(ARDUINO) && ARDUINO >=100
#include "Arduino.h"
#else
#include <WProgram.h>
#endif

// <Settings>

#if !defined(XRAMEND)
#error "XRAMEND NOT DEFINED, SORRY I CAN NOT USE THIS TOOLCHAIN!"
#endif

// Please note, you only need to disable this if you want to optimize code size, and save a few bytes of RAM.
// 1 for Rugged Circuits, anything else for Andy Brown, comment out to disable
#define EXT_RAM 1

// 0 to NOT move stack to external RAM
// 1 to move stack to external RAM
#define EXT_RAM_STACK 1
// size to reserve for the stack from external RAM, this is in addition to the on-chip RAM
// the Default is 15.5K additional, so that the malloc arena can begin on an even page.
// now if we could only define a windowed area, we could have banks AND stack. Hmmm...
#define EXT_RAM_STACK_ARENA 0x3e00
// </Settings> No user servicable parts below this point.


#include <stdlib.h>
#include <stdint.h>

namespace xmem {

	/*
	 * Pointers to the start and end of memory
	 */

#if EXT_RAM_STACK
#define XMEM_START ((void *)(XRAMEND + EXT_RAM_STACK_ARENA+1))
#define XMEM_END ((void *)0xFFFF)
#define XMEM_STACK_TOP ((void *)(XRAMEND + EXT_RAM_STACK_ARENA))
#else
#define XMEM_START ((void *)0x2200)
#define XMEM_END ((void *)0xFFFF)
#endif
	/*
	 * State variables used by the heap
	 */

	struct heapState {
			char *__malloc_heap_start;
			char *__malloc_heap_end;
			void *__brkval;
			void *__flp;
	};

	/*
	 * Results of a self-test run
	 */

	struct SelfTestResults {
			bool succeeded;
			volatile uint8_t *failedAddress;
			uint8_t failedBank;
	};

	/*
	 * Prototypes for the management functions
	 */
	void begin(bool heapInXmem_, bool stackInXmem);
	void begin(bool heapInXmem_);
	void setMemoryBank(uint8_t bank_,bool switchHeap_=true);
	SelfTestResults selfTest();
	void saveHeap(uint8_t bank_);
	void restoreHeap(uint8_t bank_);
        uint8_t getTotalBanks();

}

/*
 * References to the private heap variables
 */

extern "C" {
	extern void *__flp;
	extern void *__brkval;
}

#endif
