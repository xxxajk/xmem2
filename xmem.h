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

// Uncomment to always use.
// It is much better to do this from make The number is maximal possible tasks.
//#define USE_MULTIPLE_APP_API 16

// Please note, you only need to disable this if you want to optimize code size, and save a few bytes of RAM.
// 1 for Rugged Circuits, anything else for Andy Brown, comment out to disable
#ifndef EXT_RAM
#define EXT_RAM 1
#endif

//
// 0 to NOT move heap to external RAM
// 1 to move heap to external RAM
//

#ifndef EXT_RAM_HEAP
#define EXT_RAM_HEAP 1
#endif

// 0 to NOT move stack to external RAM
// 1 to move stack to external RAM
#ifndef EXT_RAM_STACK
#define EXT_RAM_STACK 1
#endif

#if !defined(XMEM_MULTIPLE_APP)
#if defined(USE_MULTIPLE_APP_API)
#define XMEM_MULTIPLE_APP
#endif
#endif

#if !defined(USE_MULTIPLE_APP_API)
// Size to reserve for the stack from external RAM, this is NOT in addition to the on-chip RAM.
// Now if we could only define a windowed area, we could have banks AND stack. Hmmm...
// The best place to land this is on a 4K boundary. Add 0x0f00 to start arena on an even page.
// Here is a handy table:
// value    | stack size | malloc arena start
// ---------+------------+-------------------
// 0x3f00   | 16128      |
// 0x2f00   | 12032      |
// 0x1f00   | 7936       |
// 0x0f00   | 3840       |
// < 0x0f00 | default    |
#define EXT_RAM_STACK_ARENA 0x03f00
#else
#define EXT_RAM_STACK_ARENA 0x2000 // 8K stack, 24K malloc arena. Adjust as needed.
#endif

// set to 1 to include ram test code.
#define WANT_TEST_CODE 0
// </Settings> No user serviceable parts below this point.

#if !defined(XRAMEND)
#error "XRAMEND NOT DEFINED, SORRY I CAN NOT USE THIS TOOLCHAIN!"
#endif


/*
 * Pointers to the start and end of memory
 */
#if !defined(USE_MULTIPLE_APP_API)
#if EXT_RAM_STACK && (EXT_RAM_STACK_ARENA > 0x0eff)
#define XMEM_STACK_TOP (RAMEND + EXT_RAM_STACK_ARENA)
#define XMEM_START ((void *)(XMEM_STACK_TOP + 1))
#define XMEM_END ((void *)XRAMEND)
#else
#define XMEM_START ((void *)(XRAMEND + 1))
#define XMEM_END ((void *)0xFFFF)
#endif
#else
#define CLK_prescale1       ((1 << WGM12) | (1 << CS10))
#define CLK_prescale8       ((1 << WGM12) | (1 << CS11))
#define CLK_prescale64      ((1 << WGM12) | (1 << CS10) | (1 << CS11))
#define CLK_prescale256     ((1 << WGM12) | (1 << CS12))
#define CLK_prescale1024    ((1 << WGM12) | (1 << CS12) | (1 << CS10))
#define XMEM_STACK_TOP (0x7FFF + EXT_RAM_STACK_ARENA)
#define XMEM_START ((void *)(XMEM_STACK_TOP + 1))
#define XMEM_END ((void *)0xFFFF)
#endif


#include <stdlib.h>
#include <stdint.h>

namespace xmem {

        /*
	 * State variables used by the heap
	 */
        void setupHeap(void);
	struct heapState {
			char *__malloc_heap_start;
			char *__malloc_heap_end;
			void *__brkval;
			void *__flp;
	};

	/*
	 * Prototypes for the management functions
	 */
	void begin(bool heapInXmem_, bool stackInXmem);
	void begin(bool heapInXmem_);
	void setMemoryBank(uint8_t bank_,bool switchHeap_=true);
	void saveHeap(uint8_t bank_);
	void restoreHeap(uint8_t bank_);
        uint8_t getTotalBanks();

#if defined(USE_MULTIPLE_APP_API)
        uint8_t SetupTask(void (*func)(void));
        void StartTask(uint8_t which);
        void PauseTask(uint8_t which);
        void TaskFinish(void);
#endif

#if WANT_TEST_CODE
        /*
	 * Results of a self-test run
	 */

	struct SelfTestResults {
			bool succeeded;
			volatile uint8_t *failedAddress;
			uint8_t failedBank;
	};

	SelfTestResults selfTest();
#endif
}

/*
 * References to the private heap variables
 */

extern "C" {
	extern void *__flp;
	extern void *__brkval;
}
#if defined(USE_MULTIPLE_APP_API)
extern volatile unsigned int keepstack; // original stack pointer on the avr.
#endif

#endif
