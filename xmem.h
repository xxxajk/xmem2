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
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

// <Settings>

// How much AVR ram to use for bank<->bank copier, 8 more gets added to this amount.
#ifndef _RAM_COPY_SZ
#define _RAM_COPY_SZ 256
#endif


// Uncomment to always use.
// It is much better to do this from the Makefile.
// The number is maximal possible tasks.
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
#define EXT_RAM_STACK_ARENA 0x2000 // Default 8K stack, 24K malloc arena.
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
#define CLK_prescale1           ((1 << WGM12) | (1 << CS10))
#define CLK_prescale8           ((1 << WGM12) | (1 << CS11))
#define CLK_prescale64          ((1 << WGM12) | (1 << CS10) | (1 << CS11))
#define CLK_prescale256         ((1 << WGM12) | (1 << CS12))
#define CLK_prescale1024        ((1 << WGM12) | (1 << CS12) | (1 << CS10))

// How fast to task switch? 100us is highly recommended, 10us is NOT. You've been warned.
#define hundredus
//#define fiftyus

#ifdef hundredus
// 100us
// (0.001/(1/((16 *(10^6)) / 8))) - 1 = 1999
#define CLK_CMP ((((F_CPU) / 8) / 1000) - 1)
#else
#define fiftyus
#ifdef fiftyus
// 50us
// (0.0005/(1/((16 *(10^6)) / 8))) - 1 = 999
#define CLK_CMP ((((F_CPU) / 8) / 2000) - 1)
#else
// 10us
// (0.0001/(1/((16 *(10^6)) / 8))) - 1 = 199
#define CLK_CMP ((((F_CPU) / 8) / 10000) - 1)
#endif
#endif

#define XMEM_STACK_TOP          (0x7FFF + EXT_RAM_STACK_ARENA)
#define XMEM_START              ((void *)(XMEM_STACK_TOP + 1))
#define XMEM_END                ((void *)0xFFFF)
#define XMEM_STATE_FREE         0x00 // Thread is not running, free slot to use.
#define XMEM_STATE_PAUSED       0x01 // Thread is paused.
#define XMEM_STATE_RUNNING      0x80 // Thread is running.
#define XMEM_STATE_DEAD         0x81 // Thread has stopped running.
#define XMEM_STATE_SLEEP        0x82 // Thread is sleeping.
#define XMEM_STATE_WAKE         0x83 // Thread will be woken up on next context switch.
#define XMEM_STATE_YIELD        0x84 // Thread is yielding control to another thread.
#define XMEM_STATE_HOG_CPU      0x85 // Only run this thread, Don't context switch. This is a soft CLI.
#endif

#include <stdlib.h>
#include <stdint.h>

#if defined(USE_MULTIPLE_APP_API)
        typedef struct {
                volatile unsigned int sp; // stack pointer
                volatile uint8_t state; // task state.
                volatile uint8_t parent; // the task that started this task
                volatile uint64_t sleep; // ms to sleep
        } task;

        // This structure is used to send single bytes between processes

        typedef struct {
                uint8_t volatile data; // the data to transfer
                boolean volatile ready; // data available
        } pipe_stream;

        // This structure is used to send a chunk of memory between processes

        typedef struct {
                uint8_t volatile *data; // pointer to the data available
                uint16_t volatile data_len; // length to copy
                uint8_t volatile bank; // bank the data is in
                boolean volatile ready; // data available
        } memory_stream;
#endif

        namespace xmem {

        /*
         * The currently selected bank (Also current task for multitasking)
         */

        uint8_t getcurrentBank(void);

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
        void setMemoryBank(uint8_t bank_, bool switchHeap_ = true);
        void saveHeap(uint8_t bank_);
        void restoreHeap(uint8_t bank_);
        uint8_t getTotalBanks();

#if defined(USE_MULTIPLE_APP_API)
        void Lock_Acquire(uint8_t *object);
        void Lock_Release(uint8_t *object);
        void Sleep(uint64_t sleep);
        uint8_t SetupTask(void (*func)(void), unsigned int ofs);
        uint8_t SetupTask(void (*func)(void));
        void StartTask(uint8_t which);
        void PauseTask(uint8_t which);
        void TaskFinish(void);
        void SoftCLI(void);
        void SoftSEI(void);
        uint8_t Task_Parent();
        boolean Is_Running(uint8_t which);
        boolean Is_Done(uint8_t which);
        uint8_t Task_State(uint8_t which);
        boolean Task_Is_Mine(uint8_t which);
        void Yield(void);
        boolean pipe_ready(pipe_stream *p);
        void pipe_init(pipe_stream *p);
        void pipe_put(uint8_t c, pipe_stream *p);
        uint8_t pipe_get(pipe_stream *p);
        void pipe_send_message(uint8_t *message, int len, pipe_stream *p);
        int pipe_recv_message(uint8_t **message, pipe_stream *p);
        void memory_init(memory_stream *p);
        boolean memory_ready(memory_stream *p);
        void memory_send(uint8_t *data, uint16_t len, memory_stream *p);
        uint16_t memory_recv(uint8_t **data, memory_stream *p);
        void copy_to_task(void *d, void *s, uint16_t len, uint8_t db);
        void copy_from_task(void *d, void *s, uint16_t len, uint8_t sb);
        void *safe_malloc(size_t x);
        void SoftCLI(void);
        void SoftSEI(void);


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
extern volatile unsigned int keepstack; // original stack pointer on the avr just after booting.
#endif

#endif
