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
 *     * Modifications by Andrew J. Kroll a *a*t* oo *d*o*t* ms http://dr.ea.ms/Arduino/
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


namespace xmem {

        /*
         * State for all 8 banks
         */

        struct heapState bankHeapStates[8];

        /*
         * The currently selected bank
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
        uint8_t Autosize(bool stackInXmem_) {
                uint8_t banks = 0;
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
                totalBanks = 255; // This allows us to scan
                setMemoryBank(1, false);
                volatile uint8_t *ptr = reinterpret_cast<uint8_t *>(0x2212);
                volatile uint8_t *ptr2 = reinterpret_cast<uint8_t *>(0x2221);
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

                XMCRB = 0; // need all 64K. no pins released
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

                // Auto size RAM
                totalBanks = Autosize(stackInXmem);
                // initialize the heap states

                if (totalBanks > 0) {
                        if (heapInXmem_) {
                                __malloc_heap_end = static_cast<char *>(XMEM_END);
                                __malloc_heap_start = static_cast<char *>(XMEM_START);
                                __brkval = static_cast<char *>(XMEM_START);
                        }

                        for (bank = 0; bank < totalBanks; bank++)
                                saveHeap(bank);

                        // set the current bank to zero

                        setMemoryBank(0, false);
                }
#endif
        }

        void begin(bool heapInXmem_) {
                begin(heapInXmem_, false);
        }


        /*
         * Set the memory bank
         */

        void setMemoryBank(uint8_t bank_, bool switchHeap_) {
                if (totalBanks < 2) return;

#if defined(EXT_RAM)
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
                if (totalBanks < 2) return;
#if defined(EXT_RAM)
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
                if (totalBanks < 2) return;
#if defined(EXT_RAM)
                __malloc_heap_start = bankHeapStates[bank_].__malloc_heap_start;
                __malloc_heap_end = bankHeapStates[bank_].__malloc_heap_end;
                __brkval = bankHeapStates[bank_].__brkval;
                __flp = bankHeapStates[bank_].__flp;
#endif
        }

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
                return results;
#endif
        }
}
