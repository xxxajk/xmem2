//
// NOTE to the malinformed!
// Tabs on the entire planet named Earth are 8 _SPACES_!
// Tabs are NOT 2, NOT 3, etc, and not the \t character!
//
// PLEASE STOP writing horrible to read code!
// Thanks!

/* This QuadRAM application is a production test that verifies that
 * all RAM locations are unique and can store values.
 *
 * Assumptions:
 *
 *   - QuadRAM plugged in to Arduino Mega 1280 or Mega 2560
 *     -- Modifications by Andrew J. Kroll
 *        supports PJRC.com Teensy++ 2.0
 *        continually tests, does not stop
 *
 * The behavior is as follows:
 *
 *   - Each one of 16 banks of 32 kbytes each is filled with a pseudorandom sequence of numbers
 *   - The 16 banks are read back to verify the pseudorandom sequence
 *   - On success, the Arduino's LED is turned on steadily, with a brief pulse off every 3 seconds
 *   - On failure, the Arduino's LED blinks rapidly
 *   - The serial port is used for status information (38400 bps)
 *
 * This software is licensed under the GNU General Public License (GPL) Version
 * 3 or later. This license is described at
 * http://www.gnu.org/licenses/gpl.html
 *
 * Application Version 1.0 -- October 2011 Rugged Circuits LLC
 * http://www.ruggedcircuits.com/html/quadram.html
 */

// I don't depend on IDEs including for me, and neither should you!
#include <Arduino.h>

#define NUM_BANKS 16

#ifdef CORE_TEENSY
// Settings for Teensy 2++
// Bits PF0, PF1, PF2 select the bank
#define XDDR DDRF
#define SETDDR 0x07
#define XPORT PORTF
// Select bank 0, pullups on everything else
#define SETPORT 0xF8
#define SETBANK(bnk) ((XPORT & 0xF8) | (bnk & 0x07))
// PE7 is RAM chip enable, active low.
#define ENDDR DDRE
#define ENPORT PORTE
#define SETENDDR (ENDDR | _BV(7))
#define SETENPORT (ENPORT & ~_BV(7))

#else
// Settings for Arduino Mega/Mega2560
// Bits PL7, PL6, PL5 select the bank
#define XDDR DDRL
#define SETDDR 0xE0
#define XPORT PORTL
// Select bank 0, pullups on everything else
#define SETPORT 0x1F
#define SETBANK(bnk) (XPORT & 0x1F) | ((bnk & 0x07) << 5)

// PD7 is RAM chip enable, active low.
#define ENDDR DDRD
#define SETENDDR _BV(7)
#define ENPORT PORTD
#define SETENPORT 0x7F
#endif

void setup(void) {
        XDDR = SETDDR;
        XPORT = SETPORT;

        /* Enable XMEM interface:

               SRE    (7)   : Set to 1 to enable XMEM interface
               SRL2-0 (6-4) : Set to 00x for wait state sector config: Low=N/A, High=0x2200-0xFFFF
               SRW11:0 (3-2) : Set to 00 for no wait states in upper sector
               SRW01:0 (1-0) : Set to 00 for no wait states in lower sector
         */
        XMCRA = 0x80;

        // Bus keeper, lower 7 bits of Port C used for upper address bits, bit 7 is under PIO control
        XMCRB = _BV(XMBK) | _BV(XMM0);
        DDRC |= _BV(7);
        PORTC = 0x7F; // Enable pullups on port C, select bank 0

        // Enable RAM now, and enable pullups on Port D.
        ENDDR = SETENDDR;
        ENPORT = SETENPORT;

        while(!Serial);
        delay(200);
        // Open up a serial channel
        Serial.begin(38400);
        // Enable on-board LED
        pinMode(LED_BUILTIN, OUTPUT);

        Serial.println("Beginning memory test...");
}

// Blink the Arduino LED quickly to indicate a memory test failure
//void blinkfast(void)
//{
//  digitalWrite(LED_BUILTIN, HIGH);
//  delay(100);
//  digitalWrite(LED_BUILTIN, LOW);
//  delay(100);
//}

// Provide information on which address failed. Then, sit in an infinite loop blinking
// the LED quickly.
unsigned long gTotalFailures;

void fail(uint8_t *addr, uint8_t expect, uint8_t got, uint8_t lastGood) {
        Serial.println();
        Serial.print("At address ");
        Serial.print((uint16_t)addr, HEX);
        Serial.print(" expected ");
        Serial.print(expect, HEX);
        Serial.print(" got ");
        Serial.print(got, HEX);
        Serial.print(" last good was ");
        Serial.print(lastGood, HEX);
        gTotalFailures++;

        //while (1) {
        //  blinkfast();
        //}
}

#define STARTADDR ((uint8_t *)0x8000)
#define COUNT (0x8000U-1)

long seed = 1234;

// Select one of 16 32-kilobyte banks

void banksel(uint8_t bank) {
        XPORT = SETBANK(bank);
        // Take care of highest 1 bit
        if(bank & 0x08) {
                PORTC |= _BV(7);
        } else {
                PORTC &= ~_BV(7);
        }
}

// Fill the currently selected 32 kilobyte bank
// of memory with a 'random' sequence

void bankfill(void) {
        uint8_t *addr;
        uint8_t val;
        uint16_t count;

        for(addr = STARTADDR, count = COUNT; count; addr++, count--) {
                val = (uint8_t)random(256);
                *addr = val;
                digitalWrite(LED_BUILTIN, ((count & 0x1000U) ? 0 : 1));
                //if ((count & 0xFFFU) == 0) blinkfast();
        }
}

// Check the currently selected 32 kilobyte bank
// of memory against expected values in each memory cell.

void bankcheck(void) {
        uint8_t expect;
        uint8_t *addr;
        uint16_t count;
        uint8_t lastGood = 0xEE;

        for(addr = STARTADDR, count = COUNT; count; addr++, count--) {
                expect = random(256);
                if(*addr != expect) {
                        fail(addr, expect, *addr, lastGood);
                }
                lastGood = expect;
                digitalWrite(LED_BUILTIN, ((count & 0x100U) ? 0 : 1));
                //if ((count & 0xFFFU) == 0x123) blinkfast();
        }
}

void bad_dbf(uint8_t pat) {
        Serial.println(" ");
        Serial.print("Databus Failure. Pattern=");
        Serial.print(pat);
        gTotalFailures++;
}

void bad_abf(size_t abadaddress) {
        Serial.println(" ");
        Serial.print("Addressbus Failure @ ");
        Serial.print(abadaddress);
        gTotalFailures++;
}

void bad_mf(size_t abadaddress) {
        Serial.print("Memory Failure @ ");
        Serial.print(abadaddress);
        gTotalFailures++;
}

void torture1(size_t aarena, size_t aamount) {
        Serial.print("Testing arena ");
        Serial.print(aarena, 16);
        Serial.print(" - ");
        Serial.print(aarena + aamount, 16);
        Serial.print(" (0x");
        Serial.print(aamount, 16);
        Serial.print("[");
        Serial.print(aamount, 10);
        Serial.print("] bytes)");
}

extern "C" {

        typedef unsigned char datum; /* Set the data bus width to 8 bits.  */
        uint16_t x;
        size_t originalsp;
        size_t arenaval;
        uint8_t *nsp;
        volatile datum *arena;
        volatile datum *badaddress;
        volatile datum *tbegin;
        size_t many;
        size_t flik;
        uint32_t mask;

        /**********************************************************************
         *
         * Function:    memTestDataBus()
         *
         * Description: Test the data bus wiring in a memory region by
         *              performing a walking 1's test at a fixed address
         *              within that region.  The address (and hence the
         *              memory region) is selected by the caller.
         *
         * Returns:     Nothing
         *
         **********************************************************************/
        void memTestDataBus(volatile datum * address) {
                datum pattern;


                /*
                 * Perform a walking 1's test at the given address.
                 */
                for(pattern = 1; pattern != 0; pattern <<= 1) {
                        /*
                         * Write the test pattern.
                         */
                        *address = pattern;

                        /*
                         * Read it back (immediately is okay for this test).
                         */
                        if(*address != pattern) {
                                bad_dbf(pattern);
                        }
                }

        } /* memTestDataBus() */

        /**********************************************************************
         *
         * Function:    memTestAddressBus()
         *
         * Description: Test the address bus wiring in a memory region by
         *              performing a walking 1's test on the relevant bits
         *              of the address and checking for aliasing. This test
         *              will find single-bit address failures such as stuck
         *              -high, stuck-low, and shorted pins.  The base address
         *              and size of the region are selected by the caller.
         *
         * Notes:       For best results, the selected base address should
         *              have enough LSB 0's to guarantee single address bit
         *              changes.  For example, to test a 64-Kbyte region,
         *              select a base address on a 64-Kbyte boundary.  Also,
         *              select the region size as a power-of-two--if at all
         *              possible.
         *
         * Returns:     Nothing
         *
         **********************************************************************/

        void memTestAddressBus(volatile datum * baseAddress, unsigned long nBytes) {
                unsigned long addressMask = (nBytes / sizeof (datum) - 1);
                unsigned long offset;
                unsigned long testOffset;

                datum pattern = (datum)0xAAAAAAAA;
                datum antipattern = (datum)0x55555555;


                /*
                 * Write the default pattern at each of the power-of-two offsets.
                 */
                for(offset = 1; (offset & addressMask) != 0; offset <<= 1) {
                        baseAddress[offset] = pattern;
                }

                /*
                 * Check for address bits stuck high.
                 */
                testOffset = 0;
                baseAddress[testOffset] = antipattern;

                for(offset = 1; (offset & addressMask) != 0; offset <<= 1) {
                        if(baseAddress[offset] != pattern) {
                                bad_abf((size_t)((datum *) & baseAddress[offset]));
                        }
                }

                baseAddress[testOffset] = pattern;

                /*
                 * Check for address bits stuck low or shorted.
                 */
                for(testOffset = 1; (testOffset & addressMask) != 0; testOffset <<= 1) {
                        baseAddress[testOffset] = antipattern;

                        if(baseAddress[0] != pattern) {
                                bad_abf((size_t)((datum *) & baseAddress[offset]));
                        }

                        for(offset = 1; (offset & addressMask) != 0; offset <<= 1) {
                                if((baseAddress[offset] != pattern) && (offset != testOffset)) {
                                        bad_abf((size_t)((datum *) & baseAddress[offset]));
                                }
                        }

                        baseAddress[testOffset] = pattern;
                }

        } /* memTestAddressBus() */

        /**********************************************************************
         *
         * Function:    memTestDevice()
         *
         * Description: Test the integrity of a physical memory device by
         *              performing an increment/decrement test over the
         *              entire region.  In the process every storage bit
         *              in the device is tested as a zero and a one.  The
         *              base address and the size of the region are
         *              selected by the caller.
         *
         * Returns:     Nothing
         *
         *
         **********************************************************************/
        void memTestDevice(volatile datum * baseAddress, unsigned long nBytes) {
                unsigned long offset;
                unsigned long nWords = nBytes / sizeof (datum);

                datum pattern;
                datum antipattern;


                /*
                 * Fill memory with a known pattern.
                 */
                for(pattern = 1, offset = 0; offset < nWords; pattern++, offset++) {
                        baseAddress[offset] = pattern;
                }

                /*
                 * Check each location and invert it for the second pass.
                 */
                for(pattern = 1, offset = 0; offset < nWords; pattern++, offset++) {
                        if(baseAddress[offset] != pattern) {
                                bad_mf((size_t)((datum *) & baseAddress[offset]));
                        }

                        antipattern = ~pattern;
                        baseAddress[offset] = antipattern;
                }

                /*
                 * Check each location for the inverted pattern and zero it.
                 */
                for(pattern = 1, offset = 0; offset < nWords; pattern++, offset++) {
                        antipattern = ~pattern;
                        if(baseAddress[offset] != antipattern) {
                                bad_mf((size_t)((datum *) & baseAddress[offset]));
                        }
                }

        } /* memTestDevice() */

        // Torture-test arena

        void memTest(void) {
                arena = (volatile datum *)(STARTADDR);
                torture1((size_t)arena, COUNT);
                for(x = 0; x < 100; x++) {
                        memTestDataBus(arena);
                        memTestAddressBus(arena, COUNT);
                        memTestDevice(arena, COUNT);
                        digitalWrite(LED_BUILTIN, (x & 0x01));
                }
                digitalWrite(LED_BUILTIN, LOW);
        } /* memTest() */
}

void loop(void) {
        uint8_t bank;

        Serial.println(" Starting pseudo random fill and readback test...");
        // Always start filling and checking with the same random seed
        randomSeed(seed);
        gTotalFailures = 0;
        // Fill all 16 banks with random numbers
        for(bank = 0; bank < NUM_BANKS; bank++) {
                Serial.print("  Filling bank ");
                if(bank < 10) Serial.print(" ");
                Serial.print(bank, DEC);
                Serial.println(" ...");
                banksel(bank);
                bankfill();
        }

        // Restore the initial random seed, then check all banks
        // against expected random numbers
        randomSeed(seed);
        for(bank = 0; bank < NUM_BANKS; bank++) {
                Serial.print(" Checking bank ");
                if(bank < 10) Serial.print(" ");
                Serial.print(bank, DEC);
                Serial.print(" ...");
                banksel(bank);
                bankcheck();
                Serial.println();
        }

        Serial.println(" Starting torture tests. Data bus, Address bus, and memory bit functions...");
        for(bank = 0; bank < NUM_BANKS; bank++) {
                Serial.print("  Torture bank ");
                if(bank < 10) Serial.print(" ");
                Serial.print(bank, DEC);
                Serial.print(" ... ");
                banksel(bank);
                memTest();
                Serial.println();
        }

        if(gTotalFailures) {
                Serial.print(gTotalFailures);
                Serial.println(" total failures");
        } else {
                Serial.println("Success!");
        }
        Serial.println();

        // Keep the LED on mostly, and pulse off briefly every 3 seconds
        // MMmmmm, nah! Do it again!
        //while (1) {
        //  digitalWrite(LED_BUILTIN, HIGH);
        //  delay(3000);
        //  digitalWrite(LED_BUILTIN, LOW);
        //  delay(500);
        // }
}


