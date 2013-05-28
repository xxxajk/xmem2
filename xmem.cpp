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

// Select which shield you are using, Andy Brown's or the Rugged Circuits QuadRAM Shield.
// Only uncomment one of the two lines below.

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
                        volatile uint8_t *ptr2 = reinterpret_cast<uint8_t *>(028221);
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
                volatile uint8_t *ptr2 = reinterpret_cast<uint8_t *>(028221);
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
                                        tasks[bank].state = 0; // Not in use.
                                }
#endif
                        }
                }
#if defined(XMEM_MULTIPLE_APP)
                        tasks[0].state = 0x80; // In use, running
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

        void StartTask(uint8_t which) {
                cli();
                if (tasks[which].state == 0x01) tasks[which].state = 0x80; // running
                sei();
        }

        void PauseTask(uint8_t which) {
                cli();
                if (tasks[which].state == 0x80) tasks[which].state = 0x01; // setup/pause state
                sei();
        }

        void TaskFinish(void) {
                cli();
                tasks[currentBank].state = 0x81; // dead
                sei();
                for (;;);
        }


        // Find a free slot, and start up.
        // NOTE: NOT REENTRANT! returns zero on error.
        // returns task number on success.
        // collects tasks that have ended during search.
        // r25:r24 contains func,
        // r25:24:r22 for 2560?

        uint8_t SetupTask(void (*func)(void)) {
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
                for (i = 0; i < XMEM_MAX_BANK_HEAPS; i++) {
                        if (i == currentBank) continue; // Don't free or check current task!
                        if (tasks[i].state == 0x81) tasks[i].state = 0x00; // collect a dead task.
                        if (tasks[i].state == 0x00) {
                                tasks[i].state = 0x01; // setting up/paused
                                // switch banks, and do setup
                                oldbank = currentBank;
                                oldsp = SP;
                                SP = keepstack; // original on-chip ram
                                setMemoryBank(i, false);
                                setupHeap();
                                tasks[currentBank].sp = (XMEM_STACK_TOP);
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
                                asm volatile ("push r3"); // loh8
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

        // Task switch, do what we need to do as fast as possible.
        // We do not count what we do in here as part of the task time.
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

                TIMSK3 ^= (1 << OCIE1A); // Disable timer
                register uint8_t check = currentBank;
                for(;;) {
                        check++;
                        if (check == XMEM_MAX_BANK_HEAPS) {
                                check = 0;
                        }
                        if(tasks[check].state == 0x80) break;
                }
                if (check != currentBank) { // skip if we are not context switching
                        // inline
                        bankHeapStates[currentBank].__malloc_heap_start = __malloc_heap_start;
                        bankHeapStates[currentBank].__malloc_heap_end = __malloc_heap_end;
                        bankHeapStates[currentBank].__brkval = __brkval;
                        bankHeapStates[currentBank].__flp = __flp;
                        tasks[currentBank].sp = SP;
                        SP = keepstack; // original on-chip ram
                        setMemoryBank(check, false);
                        __malloc_heap_start = bankHeapStates[currentBank].__malloc_heap_start;
                        __malloc_heap_end = bankHeapStates[currentBank].__malloc_heap_end;
                        __brkval = bankHeapStates[currentBank].__brkval;
                        __flp = bankHeapStates[currentBank].__flp;
                        SP = tasks[currentBank].sp;
                }
                // Again!
                //TCCR3A = 0; // Zero
                //TCCR3B = 0; //      timer
                TIMSK3 |= (1 << OCIE1A); // Enable timer
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
