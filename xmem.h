
#if defined(__AVR__)
/*
 * xmem.h
 *
 *  Created on: 21 Aug 2011
 *      Author: Andy Brown
 *     Website: www.andybrown.me.uk
 *
 *  This work is licensed under a Creative Commons Attribution-ShareAlike 3.0 Unported License.
 */

#if !defined(ARDUINO)
#error "Please upgrade your Arduino IDE to at least 1.0.5"
#endif
#if ARDUINO <= 104
#error "Please upgrade your Arduino IDE to at least 1.0.5"
#endif
#ifndef XMEM_H
#define XMEM_H
#ifndef AJK_NI
#define AJK_NI __attribute__((noinline))
#endif

#include <Arduino.h>
#include <stdlib.h>
#include <stdint.h>

#define XMEM_STATE_FREE         0x00 // Thread is not running, free slot to use.
#define XMEM_STATE_PAUSED       0x01 // Thread is paused.
#define XMEM_STATE_USED         0x02 // Used bank by a task.
#define XMEM_STATE_RUNNING      0x80 // Thread is running.
#define XMEM_STATE_DEAD         0x81 // Thread has stopped running.
#define XMEM_STATE_SLEEP        0x82 // Thread is sleeping.
#define XMEM_STATE_WAKE         0x83 // Thread will be woken up on next context switch.
#define XMEM_STATE_YIELD        0x84 // Thread is yielding control to another thread.
#define XMEM_STATE_HOG_CPU      0x85 // Only run this thread, Don't context switch. This is a soft CLI.

struct heapState {
                char *__malloc_heap_start;
                char *__malloc_heap_end;
                void *__brkval;
                void *__flp;
};

typedef struct {
        volatile unsigned int sp; // stack pointer
        volatile uint8_t state; // task state.
        volatile uint8_t parent; // the task that started this task
        volatile uint64_t sleep; // ms to sleep
} task;

// This structure is used to send single bytes between processes

typedef struct {
        uint8_t volatile data; // the data to transfer
        bool volatile ready; // data available
} pipe_stream;

// This structure is used to send a chunk of memory between processes

typedef struct {
        uint8_t volatile *data; // pointer to the data available
        uint16_t volatile data_len; // length to copy
        uint8_t volatile bank; // bank the data is in
        bool volatile ready; // data available
} memory_stream;

// extern volatile unsigned int keepstack; // original stack pointer on the avr just after booting.

//#ifdef __cplusplus
#if 0
namespace xmem {

        uint8_t getcurrentBank(void);
        void setupHeap(void);
        void begin(bool heapInXmem_, bool stackInXmem);
        void begin(bool heapInXmem_);
        void setMemoryBank(uint8_t bank_, bool switchHeap_ = true);
        void saveHeap(uint8_t bank_);
        void restoreHeap(uint8_t bank_);
        uint8_t getTotalBanks();

        void Lock_Acquire(volatile uint8_t *object);
        void Lock_Release(volatile uint8_t *object);
        void Sleep(uint64_t sleep);
        uint8_t SetupTask(void (*func)(void), unsigned int ofs);
        uint8_t SetupTask(void (*func)(void));
        void StartTask(uint8_t which);
        void PauseTask(uint8_t which);
        void TaskFinish(void);
        void SoftCLI(void);
        void SoftSEI(void);
        uint8_t Task_Parent();
        bool Is_Running(uint8_t which);
        bool Is_Done(uint8_t which);
        uint8_t Task_State(uint8_t which);
        bool Task_Is_Mine(uint8_t which);
        void Yield(void);
        bool pipe_ready(pipe_stream *p);
        void pipe_init(pipe_stream *p);
        void pipe_put(uint8_t c, pipe_stream *p);
        uint8_t pipe_get(pipe_stream *p);
        void pipe_send_message(uint8_t *message, int len, pipe_stream *p);
        int pipe_recv_message(uint8_t **message, pipe_stream *p);
        void memory_init(memory_stream *p);
        bool memory_ready(memory_stream *p);
        void memory_send(uint8_t *data, uint16_t len, memory_stream *p);
        uint16_t memory_recv(uint8_t **data, memory_stream *p);
        void copy_to_task(void *d, void *s, uint16_t len, uint8_t db);
        void copy_from_task(void *d, void *s, uint16_t len, uint8_t sb);
        void *safe_malloc(size_t x);
        void SoftCLI(void);
        void SoftSEI(void);
        uint8_t AllocateExtraBank(void);
        void FreeBank(uint8_t bank);

        extern uint8_t max_banks;
        extern uint8_t currentBank;
        extern volatile task tasks[];
        extern struct heapState bankHeapStates[];

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
#endif

/*
 * References to the private heap variables
 */
#ifdef __cplusplus
extern "C" {
#endif
        extern void *__flp;
        extern void *__brkval;
        unsigned int freeHeap();
#ifdef __cplusplus
}
#endif
#endif



#ifdef LOAD_XMEM

// <Settings>
// Please note, you only need to disable this if you want to optimize code size, and save a few bytes of RAM.
// 0 to disable, 1 for Rugged Circuits, 2 for Andy Brown
#ifndef EXT_RAM
#define EXT_RAM 1
#endif

// How much AVR ram to use for bank<->bank copier, 8 more gets added to this amount.
#ifndef _RAM_COPY_SZ
#define _RAM_COPY_SZ 256
#endif


// Uncomment to always use.
// It is much better to do this from the Makefile.
// The number is maximal possible tasks.
//#define USE_MULTIPLE_APP_API 16

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

#if EXT_RAM == 1
#define RUGGED_CIRCUITS_SHIELD
#elif  EXT_RAM > 1
#define ANDY_BROWN_SHIELD
#endif

#if !defined(XMEM_MULTIPLE_APP)
#define XMEM_MAX_BANK_HEAPS 8
#else
#include <avr/interrupt.h>
#define XMEM_MAX_BANK_HEAPS USE_MULTIPLE_APP_API
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

// Set to zero to save one byte of ram, reduces code size.
// Warning: Don't set to zero if you share SPI devices.
#ifndef XMEM_SPI_LOCK
#define XMEM_SPI_LOCK 1
#endif

// Set to zero to save one byte of ram, reduces code size.
// Warning: Don't set to zero if you share I2C devices.
#ifndef XMEM_I2C_LOCK
#define XMEM_I2C_LOCK 1
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

#define XMEM_STACK_TOP          (0x7FFFU + EXT_RAM_STACK_ARENA)
#define XMEM_START              ((void *)(XMEM_STACK_TOP + 1))
#define XMEM_END                ((void *)0xFFFFU)

#endif

#if defined(USE_MULTIPLE_APP_API)

#endif



#ifdef __cplusplus
#if defined(USE_MULTIPLE_APP_API)
#if XMEM_SPI_LOCK
#define XMEM_ACQUIRE_SPI(...) xmem::Lock_Acquire(&xmem_spi_lock)
#define XMEM_RELEASE_SPI(...) xmem::Lock_Release(&xmem_spi_lock)
extern volatile uint8_t xmem_spi_lock;
#else
#define XMEM_ACQUIRE_SPI(...) (void(0))
#define XMEM_RELEASE_SPI(...) (void(0))
#endif
#if XMEM_I2C_LOCK
#define XMEM_ACQUIRE_I2C(...) xmem::Lock_Acquire(&xmem_i2c_lock)
#define XMEM_RELEASE_I2C(...) xmem::Lock_Release(&xmem_i2c_lock)
extern volatile uint8_t xmem_i2c_lock;
#else
#define XMEM_ACQUIRE_I2C(...) (void(0))
#define XMEM_RELEASE_I2C(...) (void(0))
#endif
#endif
#endif
#if !defined(XMEM_LOADED)
#include "xmem.cpp"
#include "mainxmem.cpp"
#define XMEM_LOADED
#endif
#endif
#endif
