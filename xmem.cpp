/*
 * xmem.cpp
 *
 *  Created on: 21 Aug 2011
 *      Author: Andy Brown
 *     Website: www.andybrown.me.uk
 *
 *  This work is licensed under a Creative Commons Attribution-ShareAlike 3.0 Unported License.
 *
 *  Modified by Rugged Circuits LLC (25 Oct 2011) for use with the QuadRAM 512k shield:
 *     http://ruggedcircuits.com/html/quadram.html
 *
 *  Version 1.3:
 *  -----------
 *     * Modifications by Andrew J. Kroll https://github.com/xxxajk
 *     * Refactoring to make more modular.
 *     * Added ability to auto-size and future-proof.
 *              Will work on any amounts of banks.
 *              TO-DO: support a 32K (or less) bank mode.
 *
 *  Version 1.2:
 *  -----------
 *     * Fixed reference to 'bank_' in setMemoryBank().
 *     * Added include for Arduino.h/WProgram.h if using Arduino IDE.
 *     (Contributed by Adam Watson - adam@adamlwatson.com)
 *
 *  Version 1.1:
 *  -----------
 *     * begin() function modified to also set __brkval to the RAM start so it doesn't grow into the stack
 *       (thanks to Gene Reeves).
 *
 */
#include "xmem.h"

#if defined(EXT_RAM)
#if EXT_RAM == 1
#define RUGGED_CIRCUITS_SHIELD
#else
#define ANDY_BROWN_SHIELD
#endif
#endif

#if !defined(XMEM_MULTIPLE_APP)
#define XMEM_MAX_BANK_HEAPS 8
#else
#include <avr/interrupt.h>
#define XMEM_MAX_BANK_HEAPS USE_MULTIPLE_APP_API
#include <alloca.h>
volatile unsigned int keepstack; // original stack pointer on the avr.
#endif


namespace xmem {

        /*
         * State for all banks
         */

        struct heapState bankHeapStates[XMEM_MAX_BANK_HEAPS];

        /*
         * The currently selected bank (Also current task for multitasking)
         */

        uint8_t currentBank;

        /*
         * Total number of available banks
         */
        uint8_t totalBanks;

        /* Auto-size how many banks we have.
         * Note that 256 banks is not possible due to the limits of uint8_t.
         * However this code is future-proof for very large external RAM.
         */
#ifdef USE_MULTIPLE_APP_API

        struct task {
                unsigned int sp; // stack pointer
                uint8_t state; // task state.
                uint64_t sleep; // ms to sleep
                uint8_t parent; // the task that started this task
        } volatile tasks[USE_MULTIPLE_APP_API];
#endif

        uint8_t Autosize(bool stackInXmem_) {
                uint8_t banks = 0;
#if !defined(XMEM_MULTIPLE_APP)
                if (stackInXmem_) {
                        totalBanks = 255;
                        setMemoryBank(1, false);
                        setMemoryBank(0, false);
                        totalBanks = 0;
                        volatile uint8_t *ptr = reinterpret_cast<uint8_t *>(0x2212);
                        volatile uint8_t *ptr2 = reinterpret_cast<uint8_t *>(0x2221);
                        *ptr = 0x55;
                        *ptr2 = 0xAA;
                        *ptr = 0xAA;
                        *ptr2 = 0x55;
                        if (*ptr != 0xAA && *ptr2 != 0x55) return 0;
                        totalBanks = 1;
                        return 1;
                }
#endif
                totalBanks = 255; // This allows us to scan
                setMemoryBank(1, false);
#if !defined(XMEM_MULTIPLE_APP)
                volatile uint8_t *ptr = reinterpret_cast<uint8_t *>(0x2212);
                volatile uint8_t *ptr2 = reinterpret_cast<uint8_t *>(0x2221);
#else
                volatile uint8_t *ptr = reinterpret_cast<uint8_t *>(0x8000);
                volatile uint8_t *ptr2 = reinterpret_cast<uint8_t *>(0x8001);
#endif
                do {
                        setMemoryBank(banks, false);
                        *ptr = 0x55;
                        *ptr2 = 0xAA;
                        *ptr = 0xAA;
                        *ptr2 = 0x55;

                        if (*ptr != 0xAA && *ptr2 != 0x55) break;
                        if (banks != 0) {
                                setMemoryBank(0, false);
                                *ptr = banks;
                                *ptr2 = banks;
                                setMemoryBank(banks, false);
                                if (*ptr != 0xAA && *ptr2 != 0x55) break;
                        }
                        banks++;
                        // if(!banks) what do we do here for 256 or more?!
                } while (banks);
                return banks;
        }

        /*
         * Return total banks found.
         */
        uint8_t getTotalBanks() {
                return totalBanks;
        }

        /*
         * Initial setup. You must call this once
         */
        void begin(bool heapInXmem_, bool stackInXmem) {

                totalBanks = 0;

#if defined(EXT_RAM)
                uint8_t bank;

                // set up the xmem registers
#if !defined(XMEM_MULTIPLE_APP)
                XMCRB = 0; // need all 64K. no pins released
#else
                // 32KB @ 0x8000
                XMCRB = _BV(XMM0);
#endif
                XMCRA = 1 << SRE; // enable xmem, no wait states

#if defined(RUGGED_CIRCUITS_SHIELD)
                // set up the bank selector pins (address lines A16..A18)
                // these are on pins 42,43,44 (PL7,PL6,PL5). Also, enable
                // the RAM by driving PD7 (pin 38) low.

                pinMode(38, OUTPUT);
                digitalWrite(38, LOW);
                pinMode(42, OUTPUT);
                pinMode(43, OUTPUT);
                pinMode(44, OUTPUT);

#elif defined(ANDY_BROWN_SHIELD)
                // set up the bank selector pins (address lines A16..A18)
                // these are on pins 38,42,43 (PD7,PL7,PL6)

                DDRD |= _BV(PD7);
                DDRL |= (_BV(PL6) | _BV(PL7));
#endif
#if defined(XMEM_MULTIPLE_APP)
                pinMode(30, OUTPUT);
#endif

                // Auto size RAM
                totalBanks = Autosize(stackInXmem);
                // initialize the heap states

                if (totalBanks > 0) {
                        // set the current bank to zero
                        setMemoryBank(0, false);
#if !defined(XMEM_MULTIPLE_APP)
                        if (heapInXmem_)
#endif
                                setupHeap();

                        for (bank = 0; bank < totalBanks; bank++) {
                                saveHeap(bank);
#if defined(USE_MULTIPLE_APP_API)
                                if (bank < USE_MULTIPLE_APP_API) {
                                        tasks[bank].state = XMEM_STATE_FREE; // Not in use.
                                }
#endif
                        }
                }
#if defined(XMEM_MULTIPLE_APP)
                tasks[0].state = XMEM_STATE_RUNNING; // In use, running
                tasks[0].parent = 0; // ourself
#endif
#endif
        }

        void begin(bool heapInXmem_) {
                begin(heapInXmem_, EXT_RAM_STACK);
        }

        /*
         * Set the memory bank
         */

        void setMemoryBank(uint8_t bank_, bool switchHeap_) {
#if defined(EXT_RAM)
                if (totalBanks < 2) return;

                // check

                if (bank_ == currentBank)
                        return;

                // save heap state if requested

                if (switchHeap_)
                        saveHeap(currentBank);

                // switch in the new bank

#if defined(RUGGED_CIRCUITS_SHIELD)
                // Write lower 3 bits of 'bank' to upper 3 bits of Port L
                PORTL = (PORTL & 0x1F) | ((bank_ & 0x7) << 5);
#elif defined(ANDY_BROWN_SHIELD)
                if ((bank_ & 1) != 0)
                        PORTD |= _BV(PD7);
                else
                        PORTD &= ~_BV(PD7);

                if ((bank_ & 2) != 0)
                        PORTL |= _BV(PL7);
                else
                        PORTL &= ~_BV(PL7);

                if ((bank_ & 4) != 0)
                        PORTL |= _BV(PL6);
                else
                        PORTL &= ~_BV(PL6);
#endif
#if defined(XMEM_MULTIPLE_APP)
                if (bank_ & (1 << 3)) {
                        digitalWrite(30, HIGH);
                } else {
                        digitalWrite(30, LOW);
                }
#endif

                // save state and restore the malloc settings for this bank

                currentBank = bank_;

                if (switchHeap_)
                        restoreHeap(currentBank);
#endif
        }

        /*
         * Save the heap variables
         */

        void saveHeap(uint8_t bank_) {
#if defined(EXT_RAM)
                if (totalBanks < 2 || bank_ >= XMEM_MAX_BANK_HEAPS) return;
                bankHeapStates[bank_].__malloc_heap_start = __malloc_heap_start;
                bankHeapStates[bank_].__malloc_heap_end = __malloc_heap_end;
                bankHeapStates[bank_].__brkval = __brkval;
                bankHeapStates[bank_].__flp = __flp;
#endif
        }

        /*
         * Restore the heap variables
         */

        void restoreHeap(uint8_t bank_) {
#if defined(EXT_RAM)
                if (totalBanks < 2 || bank_ >= XMEM_MAX_BANK_HEAPS) return;
                __malloc_heap_start = bankHeapStates[bank_].__malloc_heap_start;
                __malloc_heap_end = bankHeapStates[bank_].__malloc_heap_end;
                __brkval = bankHeapStates[bank_].__brkval;
                __flp = bankHeapStates[bank_].__flp;
#endif
        }

        void setupHeap(void) {
                __malloc_heap_end = static_cast<char *>(XMEM_END);
                __malloc_heap_start = static_cast<char *>(XMEM_START);
                __brkval = static_cast<char *>(XMEM_START);

        }

#if defined(XMEM_MULTIPLE_APP)

        // controlled memory transaction processing

        /**
         *
         * @param p memory copier pipe in AVR RAM to initialize
         */
        void memory_init(memory_stream *p) {
                p->ready = false;
        }

        /**
         *
         * @param p memory copier pipe in AVR RAM
         * @return status
         */
        boolean memory_ready(memory_stream *p) {
                cli();
                boolean rv = p->ready;
                sei();
                return rv;
        }

        /**
         *
         * @param data data to copy
         * @param len length of the data
         * @param p memory copier pipe in AVR RAM
         */
        void memory_send(uint8_t *data, int len, memory_stream *p) {
                while (p->ready); // Since multiple senders are possible, wait.
                cli();
                p->data = data;
                p->data_len = len;
                p->bank = currentBank;
                p->ready = true;
                sei();
                while (p->ready); // Wait here, because caller may free right after!
        }

        /**
         *
         * @param data pointer to be allocated
         * @param p memory copier pipe in AVR RAM
         * @return length of message
         */
        int memory_recv(uint8_t **data, memory_stream *p) {
                *data = (uint8_t *)malloc(p->data_len);
                while (!p->ready);
                copy_from_task(*data, (void *)(p->data), p->data_len, p->bank);
                cli();
                int len = p->data_len;
                p->ready = false;
                sei();
                return len;
        }





        // byte based pipes, much like un*x

        /**
         *
         * @param message message to send
         * @param len length of message
         * @param p pipe to send message on in AVR RAM
         */
        void pipe_send_message(uint8_t *message, int len, pipe_stream *p) {
                xmem::pipe_put(len & 0xff, p);
                xmem::pipe_put((len >> 8) & 0xff, p);
                while (len) {
                        xmem::pipe_put(*message, p);
                        message++;
                        len--;
                }
        }

        /**
         *
         * @param message pointer to be allocated
         * @param p pipe to receive data from in AVR RAM
         * @return length of message
         */
        int pipe_recv_message(uint8_t **message, pipe_stream *p) {
                int len;
                int rv;
                len = xmem::pipe_get(p);
                len += xmem::pipe_get(p) << 8;
                rv = len;
                *message = (uint8_t *)malloc(len);
                uint8_t *ptr = *message;
                while (len) {
                        *ptr = xmem::pipe_get(p);
                        ptr++;
                        len--;
                }
                return rv;
        }

        /**
         *
         * @param p pipe in AVR RAM to check
         * @return status
         */
        boolean pipe_ready(pipe_stream *p) {
                cli();
                boolean rv = p->ready;
                sei();
                return rv;
        }

        /**
         *
         * @param p pipe in AVR RAM to initialize
         *
         *  FIX-ME:
         *      Make pipe depth > 1 byte.
         *      This is horribly inefficient, but it does work.
         *      The question is, how deep should a pipe be?
         *      Depth can be variable by placing the pointer to the buffer in
         *      AVR ram. i.e. init with an pointer to buffer, and size.
         */
        void pipe_init(pipe_stream *p) {
                p->ready = false;
        }

        /**
         *
         * @param c 8bit data
         * @param p pipe in AVR RAM
         */
        void pipe_put(uint8_t c, pipe_stream *p) {
                while (p->ready);
                p->data = c;
                p->ready = true;
        }

        /**
         *
         * @param p pipe in AVR RAM
         * @return 8bit data
         */
        uint8_t pipe_get(pipe_stream *p) {
                uint8_t c;
                while (!p->ready);
                c = p->data;
                p->ready = false;
                return c;
        }

#define _MEM_COPY_SZ 128

        /**
         *
         * @param d destination address
         * @param s source address
         * @param len length to copy
         * @param db destination task
         */
        void copy_to_task(void *d, void *s, size_t len, uint8_t db) {
                cli();
                tasks[currentBank].sp = SP;

                // dirty tricks department, simulate alloca
                SP = keepstack - (1 + _MEM_COPY_SZ); // original on-chip ram
                register void *buf;
                buf = (void *)(keepstack - _MEM_COPY_SZ);
                register uint8_t mb = currentBank;
                register size_t l;
                while (len) {
                        l = len;
                        if (l > _MEM_COPY_SZ) l = _MEM_COPY_SZ;
                        setMemoryBank(mb, false);
                        memcpy(buf, s, l);
                        setMemoryBank(db, false);
                        memcpy(d, buf, l);
                        len -= l;
                        s = l + ((char *)s);
                        d = l + ((char *)d);
                }
                setMemoryBank(mb, false);
                SP = tasks[mb].sp;
                sei();
        }

        /**
         *
         * @param d destination address
         * @param s source address
         * @param len length to copy
         * @param sb source task
         */
        void copy_from_task(void *d, void *s, size_t len, uint8_t sb) {
                cli();
                tasks[currentBank].sp = SP;

                // dirty tricks department, simulate alloca
                SP = keepstack - (1 + _MEM_COPY_SZ); // original on-chip ram
                register void *buf;
                buf = (void *)(keepstack - _MEM_COPY_SZ);

                register uint8_t mb = currentBank;
                register size_t l;
                while (len) {
                        l = len;
                        if (l > _MEM_COPY_SZ) l = _MEM_COPY_SZ;
                        setMemoryBank(sb, false);
                        memcpy(buf, s, l);
                        setMemoryBank(mb, false);
                        memcpy(d, buf, l);
                        len -= l;
                        s = l + ((char *)s);
                        d = l + ((char *)d);
                }
                SP = tasks[mb].sp;
                sei();
        }

        /**
         * @return which task started this task.
         */
        uint8_t Task_Parent() {
                return tasks[currentBank].parent;
        }

        /**
         *
         * @param which task ID
         * @return is the child process is running.
         *
         * Only the parent process should call this.
         */
        boolean Is_Running(uint8_t which) {
                if (tasks[which].parent != currentBank) return false;
                return (tasks[which].state == XMEM_STATE_RUNNING || tasks[which].state == XMEM_STATE_SLEEP);
        }

        /**
         *
         * @param which task ID
         * @return the child process is done.
         *
         * Only the parent should call this.
         *
         */
        boolean Is_Done(uint8_t which) {
                if (tasks[which].parent != currentBank) return true;
                return (tasks[which].state == XMEM_STATE_DEAD);
        }

        /**
         *
         * @param which task ID
         * @return state of this task.
         */
        uint8_t Task_State(uint8_t which) {
                return tasks[currentBank].state;
        }

        /**
         *
         * @param which task ID
         * @return task is a child of calling task.
         *
         */
        boolean Task_Is_Mine(uint8_t which) {
                return (tasks[which].parent == currentBank);
        }

        /**
         * Yield CPU to another task.
         */
        void Yield(void) {
                cli();
                register uint16_t rightnow = TCNT3 + 2;
                TCNT3 = CLK_CMP - 1;
                sei();
                delayMicroseconds(10); // 5 microseconds * 2 == 2 ticks
                //delayMicroseconds(5);
                cli();
                // Correct the clock as best we can after the forced tick
                if (rightnow >= CLK_CMP) rightnow = CLK_CMP - 1;
                TCNT3 = rightnow;
                sei();
        }

        /**
         *
         * @param object Caution, must be in AVR RAM
         */
        void Lock_Acquire(uint8_t *object) {
spin:
                while (*object) Yield();
                cli();
                // Just in case...
                if (*object) {
                        sei();
                        goto spin;
                }
                *object = 1;
                sei();
        }

        /**
         *
         * @param object Caution, must be in AVR RAM
         */
        void Lock_Release(uint8_t *object) {
                cli();
                *object = 0;
                sei();
        }

        /**
         * @param sleep how long to sleep for in milliseconds.
         *
         * 10us setting (default):
         * Accuracy +/- 10us + context switch, and also depends on how many tasks are woken at the same time.
         *
         * The limit for 10us is 0ms to 1844674407370955161ms.
         * 1,844,674,407,370,955,161ms = ~21,350,398,233,460.12 days, or ~58,454,204,609 years.
         * By then, you will be dead and the star known as Sol should have
         * exploded 50 to 54 billion years ago, give or take a billion years.
         *
         * 50us setting:
         * Accuracy +/- 50us + context switch, and also depends on how many tasks are woken at the same time.
         *
         * The limit for 50us is 0ms to 9223372036854775807ms.
         * 9,223,372,036,854,775,807ms = ~106,751,991,167,300.64 days, or ~292,271,023,045 years.
         * By then, you will be dead and the star known as Sol should have
         * exploded 284 to 288 billion years ago, give or take a billion years.
         *
         */
        void Sleep(uint64_t sleep) {
                if (sleep == 0) return; // what? :-)
                cli();
#ifdef fiftyus
                tasks[currentBank].sleep = sleep * 2;
#else
                tasks[currentBank].sleep = sleep * 10;
#endif
                tasks[currentBank].state = XMEM_STATE_SLEEP;
                sei();
                Yield();
                while (tasks[currentBank].state == XMEM_STATE_SLEEP); // Guard, just in case...
        }

        /**
         *
         * @param which Task ID to start or continue after pausing.
         *
         * If task is not a child of this parent, do nothing.
         *
         */
        void StartTask(uint8_t which) {
                if (tasks[which].parent != currentBank) return;
                cli();
                if (tasks[which].state == XMEM_STATE_PAUSED) tasks[which].state = XMEM_STATE_RUNNING; // running
                sei();
        }

        /**
         *
         * @param which Task ID to pause
         *
         * If task is not a child of this parent, do nothing.
         *
         */
        void PauseTask(uint8_t which) {
                if (tasks[which].parent != currentBank) return;
                cli();
                if (tasks[which].state == XMEM_STATE_RUNNING) tasks[which].state = XMEM_STATE_PAUSED; // setup/pause state
                sei();
        }

        /**
         * This is called when a task has exited.
         */
        void TaskFinish(void) {
                cli();
                tasks[currentBank].state = XMEM_STATE_DEAD; // dead
                sei();
                for (;;);
        }

        // use the defaults from xmem.h, 8192 bytes

        /**
         *
         * @param func function that becomes a new task.
         * @return Task ID
         *
         * Uses the defaults from xmem.h, 8192 bytes
         */
        uint8_t SetupTask(void (*func)(void)) {
                return SetupTask(func, (EXT_RAM_STACK_ARENA));
        }

        /**
         *
         * @param func function that becomes a new task.
         * @param ofs How much space to reserve for the stack, 1024-30719
         * @return Task ID, A task ID of 0 = failure.
         *
         * NOTE: NOT REENTRANT! Only one task at a time may allocate a new task.
         * Collects tasks that have ended during search.
         */
        uint8_t SetupTask(void (*func)(void), unsigned int ofs) {
                // r25:r24 contains func on 128x,
                // r25:24:r22 for 256x?
                static unsigned int oldsp;
                static uint8_t oldbank;
                static int i;
                cli();
                asm volatile ("push r24"); // lo8
                asm volatile ("pop r4");
                asm volatile ("push r25"); // hi 8
                asm volatile ("pop r5");
#if defined (__AVR_ATmega2560__)
                asm volatile ("push r22"); // loh8
                asm volatile ("pop r3");
#endif
                if (ofs > 30719 || ofs < 1024) {
                        sei();
                        return 0; // FAIL!
                }
                for (i = 0; i < XMEM_MAX_BANK_HEAPS; i++) {
                        if (i == currentBank) continue; // Don't free or check current task!
                        if (tasks[i].state == XMEM_STATE_DEAD) tasks[i].state = XMEM_STATE_FREE; // collect a dead task.
                        if (tasks[i].state == XMEM_STATE_FREE) {
                                tasks[i].state = XMEM_STATE_PAUSED; // setting up/paused
                                tasks[i].parent = currentBank; // parent
                                tasks[i].sp = (0x7FFF + ofs);
                                // switch banks, and do setup
                                oldbank = currentBank;
                                oldsp = SP;
                                SP = keepstack; // original on-chip ram
                                setMemoryBank(i, false);
                                setupHeap();
                                SP = tasks[currentBank].sp;

                                // Push task ending address. We lose r16 :-(
                                asm volatile("ldi r16, lo8(%0)" ::"i" (TaskFinish));
                                asm volatile ("push r16");
                                // hi8
                                asm volatile("ldi r16, hi8(%0)" ::"i" (TaskFinish));
                                asm volatile ("push r16");
#if defined (__AVR_ATmega2560__)
                                // hh8
                                asm volatile("ldi r16, hh8(%0)" ::"i" (TaskFinish));
                                asm volatile ("push r16");
#endif
                                // push function address
                                asm volatile ("push r4"); // lo8
                                asm volatile ("push r5"); // hi8
#if defined (__AVR_ATmega2560__)
                                asm volatile ("push r3"); // hh8
#endif

                                // fill in all registers.
                                // I save all because I may want to do tricks later on.
                                asm volatile ("push r1");
                                asm volatile ("push r0");
                                asm volatile ("in r0, __SREG__");
                                asm volatile ("cli");
                                asm volatile ("push r0");
                                asm volatile ("in r0 , 0x3b");
                                asm volatile ("push r0");
                                asm volatile ("in r0 , 0x3c");
                                asm volatile ("push r0");
                                asm volatile ("push r2");
                                asm volatile ("push r3");
                                asm volatile ("push r4");
                                asm volatile ("push r5");
                                asm volatile ("push r6");
                                asm volatile ("push r7");
                                asm volatile ("push r8");
                                asm volatile ("push r9");
                                asm volatile ("push r10");
                                asm volatile ("push r11");
                                asm volatile ("push r12");
                                asm volatile ("push r13");
                                asm volatile ("push r14");
                                asm volatile ("push r15");
                                asm volatile ("push r16");
                                asm volatile ("push r17");
                                asm volatile ("push r18");
                                asm volatile ("push r19");
                                asm volatile ("push r20");
                                asm volatile ("push r21");
                                asm volatile ("push r22");
                                asm volatile ("push r23");
                                asm volatile ("push r24");
                                asm volatile ("push r25");
                                asm volatile ("push r26");
                                asm volatile ("push r27");
                                asm volatile ("push r28");
                                asm volatile ("push r29");
                                asm volatile ("push r30");
                                asm volatile ("push r31");
                                tasks[currentBank].sp = SP;
                                SP = keepstack; // original on-chip ram
                                setMemoryBank(oldbank, false);
                                SP = oldsp; // original stack
                                sei();
                                return i; // success
                        }
                }
                // If we get here, we are out of task slots!
                sei();
                return 0; // fail, task zero is always taken.
        }

        // Task switch ISR, do what we need to do as fast as possible.
        // We count what we do in here as part of the task time.
        // Therefore, actual task time is variable.

        ISR(TIMER3_COMPA_vect, ISR_NAKED) {
                asm volatile ("push r1");
                asm volatile ("push r0");
                asm volatile ("in r0, __SREG__");
                asm volatile ("cli");
                asm volatile ("push r0");
                asm volatile ("in r0 , 0x3b");
                asm volatile ("push r0");
                asm volatile ("in r0 , 0x3c");
                asm volatile ("push r0");
                asm volatile ("push r2");
                asm volatile ("push r3");
                asm volatile ("push r4");
                asm volatile ("push r5");
                asm volatile ("push r6");
                asm volatile ("push r7");
                asm volatile ("push r8");
                asm volatile ("push r9");
                asm volatile ("push r10");
                asm volatile ("push r11");
                asm volatile ("push r12");
                asm volatile ("push r13");
                asm volatile ("push r14");
                asm volatile ("push r15");
                asm volatile ("push r16");
                asm volatile ("push r17");
                asm volatile ("push r18");
                asm volatile ("push r19");
                asm volatile ("push r20");
                asm volatile ("push r21");
                asm volatile ("push r22");
                asm volatile ("push r23");
                asm volatile ("push r24");
                asm volatile ("push r25");
                asm volatile ("push r26");
                asm volatile ("push r27");
                asm volatile ("push r28");
                asm volatile ("push r29");
                asm volatile ("push r30");
                asm volatile ("push r31");

                register uint8_t check;
                register uint8_t balance;

                for (check = 0; check < XMEM_MAX_BANK_HEAPS; check++) {
                        if (tasks[check].state == XMEM_STATE_SLEEP) {
                                if (tasks[check].sleep == 0) {
                                        tasks[check].state = XMEM_STATE_RUNNING;
                                        break;
                                }
                                tasks[check].sleep--;
                        }
                }

                if (check == XMEM_MAX_BANK_HEAPS) {
                        // Check tasks after current.
                        for (check = currentBank + 1; check < XMEM_MAX_BANK_HEAPS; check++) {
                                if (tasks[check].state == XMEM_STATE_RUNNING) break;
                        }
                } else {
                        // Decrement the remaining sleep counters.
                        for (balance = check + 1; balance < XMEM_MAX_BANK_HEAPS; balance++) {
                                if (tasks[check].state == XMEM_STATE_SLEEP) {
                                        if (tasks[check].sleep == 0) continue;
                                        else tasks[check].sleep--;
                                }
                        }

                }
                if (check == XMEM_MAX_BANK_HEAPS) {
                        // Check tasks before current.
                        for (check = 0; check < currentBank; check++) {
                                if (tasks[check].state == XMEM_STATE_RUNNING) break;
                        }
                }


                if (check != currentBank) { // skip if we are not context switching
                        tasks[currentBank].sp = SP;
                        SP = keepstack; // original on-chip ram
                        setMemoryBank(check, true);
                        SP = tasks[currentBank].sp;
                }
                // Again!
                asm volatile ("pop r31");
                asm volatile ("pop r30");
                asm volatile ("pop r29");
                asm volatile ("pop r28");
                asm volatile ("pop r27");
                asm volatile ("pop r26");
                asm volatile ("pop r25");
                asm volatile ("pop r24");
                asm volatile ("pop r23");
                asm volatile ("pop r22");
                asm volatile ("pop r21");
                asm volatile ("pop r20");
                asm volatile ("pop r19");
                asm volatile ("pop r18");
                asm volatile ("pop r17");
                asm volatile ("pop r16");
                asm volatile ("pop r15");
                asm volatile ("pop r14");
                asm volatile ("pop r13");
                asm volatile ("pop r12");
                asm volatile ("pop r11");
                asm volatile ("pop r10");
                asm volatile ("pop r9");
                asm volatile ("pop r8");
                asm volatile ("pop r7");
                asm volatile ("pop r6");
                asm volatile ("pop r5");
                asm volatile ("pop r4");
                asm volatile ("pop r3");
                asm volatile ("pop r2");
                asm volatile ("pop r0");
                asm volatile ("out 0x3c , r0");
                asm volatile ("pop r0");
                asm volatile ("out 0x3b , r0");
                asm volatile ("pop r0");
                asm volatile ("out __SREG__ , r0");
                asm volatile ("pop r0");
                asm volatile ("pop r1");
                asm volatile ("reti");
        }

#else

#if WANT_TEST_CODE

        /*
         * Self test the memory. This will destroy the entire content of all
         * memory banks so don't use it except in a test scenario.
         */

        SelfTestResults selfTest() {
                SelfTestResults results;
                if (totalBanks == 0) {
                        results.succeeded = false;
                        results.failedBank = 0;
                        results.failedAddress = reinterpret_cast<uint8_t *>(0x2200);
                        return results;
                }
#if defined(EXT_RAM)
                volatile uint8_t *ptr;
                uint8_t bank, writeValue, readValue;
                // write an ascending sequence of 1..237 running through
                // all memory banks
                writeValue = 1;
                for (bank = 0; bank < totalBanks; bank++) {

                        setMemoryBank(bank);

                        for (ptr = reinterpret_cast<uint8_t *>(0xFFFF); ptr >= reinterpret_cast<uint8_t *>(0x2200); ptr--) {
                                *ptr = writeValue;

                                if (writeValue++ == 237)
                                        writeValue = 1;
                        }
                }

                // verify the writes

                writeValue = 1;
                for (bank = 0; bank < 8; bank++) {

                        setMemoryBank(bank);

                        for (ptr = reinterpret_cast<uint8_t *>(0xFFFF); ptr >= reinterpret_cast<uint8_t *>(0x2200); ptr--) {

                                readValue = *ptr;

                                if (readValue != writeValue) {
                                        results.succeeded = false;
                                        results.failedAddress = ptr;
                                        results.failedBank = bank;
                                        return results;
                                }

                                if (writeValue++ == 237)
                                        writeValue = 1;
                        }
                }

                results.succeeded = true;
#endif
                return results;
        }
#endif
#endif
}
