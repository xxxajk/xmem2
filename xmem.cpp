#if defined(__AVR__)

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
 *  Version 2.0:
 *  -----------
 *     * Multitasking support is now considered finished.
 *     * Support Teensy++ 2.0
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
#if defined(XMEM_H) && !defined(XMEM_LOADED)

#if defined(XMEM_MULTIPLE_APP)
volatile unsigned int keepstack; // original stack pointer on the avr.

volatile uint32_t isr_6_push;
static char cpybuf[_RAM_COPY_SZ + 8]; // copy buffer for bank <-> bank copy
#endif

#if XMEM_SPI_LOCK
volatile uint8_t xmem_spi_lock = 0;
#endif
#if XMEM_I2C_LOCK
volatile uint8_t xmem_i2c_lock = 0;
#endif
#define XMEM_SOFT_CLI() xmem::SoftCLI()
#define XMEM_SOFT_SEI() xmem::SoftSEI()
extern "C" {

        extern unsigned int __heap_start;
        extern void *__brkval;
        extern void *__flp;

        unsigned int AJK_NI getHeapend() {

                if((unsigned int)__brkval == 0) {
                        return (unsigned int)&__heap_start;
                } else {
                        return (unsigned int)__brkval;
                }
        }

        unsigned int freeHeap() {
                if(SP < (unsigned int)__malloc_heap_start) {
                        return ((unsigned int)__malloc_heap_end - getHeapend());
                } else {
                        return (SP - getHeapend());
                }
        }
}

namespace xmem {

        uint8_t max_banks = XMEM_MAX_BANK_HEAPS;

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

        /*
         * Multitasking tracking
         */
#ifdef USE_MULTIPLE_APP_API
        volatile task tasks[USE_MULTIPLE_APP_API];
#endif

        /*
         * Save the heap variables
         */

        void AJK_NI saveHeap(uint8_t bank_) {
#if EXT_RAM
                if(totalBanks < 2 || bank_ >= XMEM_MAX_BANK_HEAPS) return;
                bankHeapStates[bank_].__malloc_heap_start = __malloc_heap_start;
                bankHeapStates[bank_].__malloc_heap_end = __malloc_heap_end;
                bankHeapStates[bank_].__brkval = __brkval;
                bankHeapStates[bank_].__flp = __flp;
#endif
        }

        /*
         * Restore the heap variables
         */

        void AJK_NI restoreHeap(uint8_t bank_) {
#if EXT_RAM
                if(totalBanks < 2 || bank_ >= XMEM_MAX_BANK_HEAPS) return;
                __malloc_heap_start = bankHeapStates[bank_].__malloc_heap_start;
                __malloc_heap_end = bankHeapStates[bank_].__malloc_heap_end;
                __brkval = bankHeapStates[bank_].__brkval;
                __flp = bankHeapStates[bank_].__flp;
#endif
        }

        void AJK_NI setupHeap(void) {
                __malloc_heap_end = static_cast<char *>(XMEM_END);
                __malloc_heap_start = static_cast<char *>(XMEM_START);
                __brkval = static_cast<char *>(XMEM_START);

        }

        /*
         * Set the memory bank
         */

        void AJK_NI setMemoryBank(uint8_t bank_, bool switchHeap_) {
#if EXT_RAM
                if(totalBanks < 2) return;

                // check

                if(bank_ == currentBank)
                        return;

                // save heap state if requested

                if(switchHeap_)
                        saveHeap(currentBank);

                // switch in the new bank
#if defined(CORE_TEENSY)
                // Write lower 3 bits of 'bank' to Port F
                PORTF = (PORTF & 0xF8) | (bank_ & 0x7);
#elif defined(RUGGED_CIRCUITS_SHIELD)
                // Write lower 3 bits of 'bank' to upper 3 bits of Port L
                PORTL = (PORTL & 0x1F) | ((bank_ & 0x7) << 5);
#elif defined(ANDY_BROWN_SHIELD)
                if((bank_ & 1) != 0)
                        PORTD |= _BV(PD7);
                else
                        PORTD &= ~_BV(PD7);

                if((bank_ & 2) != 0)
                        PORTL |= _BV(PL7);
                else
                        PORTL &= ~_BV(PL7);

                if((bank_ & 4) != 0)
                        PORTL |= _BV(PL6);
                else
                        PORTL &= ~_BV(PL6);
#endif
#if defined(XMEM_MULTIPLE_APP)
                if(bank_ & (1 << 3)) {
                        PORTC |= _BV(PC7);
                } else {
                        PORTC &= ~_BV(PC7);
                }
#endif

                // save state and restore the malloc settings for this bank

                currentBank = bank_;

                if(switchHeap_)
                        restoreHeap(bank_);
#endif
        }

        /* Auto-size how many banks we have.
         * Note that 256 banks is not possible due to the limits of uint8_t.
         * However this code is future-proof for very large external RAM.
         */
        uint8_t AJK_NI getcurrentBank(void) {
                return currentBank;
        }

        uint8_t AJK_NI Autosize(bool stackInXmem_) {
                uint8_t banks = 0;
#if !defined(XMEM_MULTIPLE_APP)
                if(stackInXmem_) {
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
                        if(*ptr != 0xAA && *ptr2 != 0x55) return 0;
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

                        if(*ptr != 0xAA && *ptr2 != 0x55) break;
                        if(banks != 0) {
                                setMemoryBank(0, false);
                                *ptr = banks;
                                *ptr2 = banks;
                                setMemoryBank(banks, false);
                                if(*ptr != 0xAA && *ptr2 != 0x55) break;
                        }
                        banks++;
                        // if(!banks) what do we do here for 256 or more?!
                } while(banks);
                return banks;
        }

        /*
         * Return total banks found.
         */
        uint8_t AJK_NI getTotalBanks() {
                return totalBanks;
        }

        /*
         * Initial setup. You must call this once
         */
        void AJK_NI begin(bool heapInXmem_, bool stackInXmem) {

                totalBanks = 0;

#if EXT_RAM
                uint8_t bank;

                // set up the xmem registers
#if !defined(XMEM_MULTIPLE_APP)
                XMCRB = 0; // need all 64K. no pins released
#else
                // 32KB @ 0x8000
                XMCRB = _BV(XMM0);
#endif
                XMCRA = 1 << SRE; // enable xmem, no wait states

#if defined(CORE_TEENSY)
                // Teensy++ 2.0 uses PE7 (pin 19) for /CS
                // and 38,39,40  (PF0, PF1, PF2) as A16 A17 A18
                // This is how it is connected regardless of
                // what RAM interface you connect, PERIOD!
                pinMode(19, OUTPUT);
                PORTE &= ~_BV(PE7);
                pinMode(38, OUTPUT);
                pinMode(39, OUTPUT);
                pinMode(40, OUTPUT);
#elif defined(RUGGED_CIRCUITS_SHIELD)
                // set up the bank selector pins (address lines A16..A18)
                // these are on pins 44,43,42 (PL5,PL6,PL7). Also, enable
                // the RAM by driving PD7 (pin 38) low.
                pinMode(38, OUTPUT);
                PORTD &= ~_BV(PD7);
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
                // Enable software IRQ pin hack.
#if defined(CORE_TEENSY)
                pinMode(18, OUTPUT);
#else
                pinMode(30, OUTPUT);
#endif
#endif

                // Auto size RAM
                totalBanks = Autosize(stackInXmem);
                // initialize the heap states

                if(totalBanks > 0) {
                        // set the current bank to zero
                        setMemoryBank(0, false);
#if !defined(XMEM_MULTIPLE_APP)
                        if(heapInXmem_)
#endif
                                setupHeap();

                        for(bank = 0; bank < totalBanks; bank++) {
                                saveHeap(bank);
#if !defined(XMEM_MULTIPLE_APP)
                                if(heapInXmem_)
#endif
                                        bankHeapStates[bank].__flp = NULL; // All new arena stuff!

#if defined(USE_MULTIPLE_APP_API)
                                if(bank < USE_MULTIPLE_APP_API) {
                                        tasks[bank].state = XMEM_STATE_FREE; // Not in use.
                                }
#endif
                        }
                }
#if defined(XMEM_MULTIPLE_APP)
                tasks[0].state = XMEM_STATE_RUNNING; // In use, running
                tasks[0].parent = 0; // ourself
#endif
#if XMEM_SPI_LOCK
                xmem_spi_lock = 0;
#endif
#if XMEM_I2C_LOCK
                xmem_i2c_lock = 0;
#endif
#endif
        }

        void AJK_NI begin(bool heapInXmem_) {
                begin(heapInXmem_, EXT_RAM_STACK);
        }

        // Switch memory bank, but do not record the bank

        void AJK_NI flipBank(uint8_t bank_) {
#if EXT_RAM
                // switch in the new bank
#if defined(CORE_TEENSY)
                // Write lower 3 bits of 'bank' to Port F
                PORTF = (PORTF & 0xF8) | (bank_ & 0x7);
#elif defined(RUGGED_CIRCUITS_SHIELD)
                // Write lower 3 bits of 'bank' to upper 3 bits of Port L
                PORTL = (PORTL & 0x1F) | ((bank_ & 0x7) << 5);
#elif defined(ANDY_BROWN_SHIELD)
                if((bank_ & 1) != 0)
                        PORTD |= _BV(PD7);
                else
                        PORTD &= ~_BV(PD7);

                if((bank_ & 2) != 0)
                        PORTL |= _BV(PL7);
                else
                        PORTL &= ~_BV(PL7);

                if((bank_ & 4) != 0)
                        PORTL |= _BV(PL6);
                else
                        PORTL &= ~_BV(PL6);
#endif
#if defined(XMEM_MULTIPLE_APP)
                if(bank_ & (1 << 3)) {
                        PORTC |= _BV(PC7);
                } else {
                        PORTC &= ~_BV(PC7);
                }
#endif
#endif
        }


#if defined(XMEM_MULTIPLE_APP)

        /**
         * Yield CPU to another task.
         */
        void AJK_NI Yield(void) {
                noInterrupts();
                if(tasks[currentBank].state == XMEM_STATE_RUNNING) tasks[currentBank].state = XMEM_STATE_YIELD;
                interrupts();
                // GAH! Arduino's interrupt cruft fucks us, so we can't do it this way.
                // PORTE ^= _BV(PE6); // forced IRQ :-D
                TCNT3 = CLK_CMP - 1;
                while(tasks[currentBank].state == XMEM_STATE_YIELD); // Arduino makes us waste CPU :-()
        }



        /* coming soon...
        void __attribute__ ((noinline)) Dump_one(int id) {
                noInterrupts();
                register uint16_t start = bankHeapStates[id].__malloc_heap_start;
                register uint16_t end = bankHeapStates[id].__malloc_heap_end;
                register uint16_t bk = bankHeapStates[id].__brkval;
                register uint16_t sk = ...
                interrupts();
        }
         *
         */

        // controlled memory transaction processing

        /**
         *
         * @param p memory copier pipe in AVR RAM to initialize
         */
        void AJK_NI memory_init(memory_stream *p) {
                p->ready = false;
        }

        /**
         *  Soft cli()-- Disallows context switching to another task.
         *  This allows for large operations such as copy of memory areas,
         *  so that the Arduino's millis() gets updated, and other IRQ can happen.
         */
        void AJK_NI SoftCLI(void) {
                noInterrupts();
                tasks[currentBank].state = XMEM_STATE_HOG_CPU;
                interrupts();
        }

        /**
         *  Soft sei()-- Allows context switching to another task.
         *  @see SoftCLI
         */
        void AJK_NI SoftSEI(void) {
                noInterrupts();
                tasks[currentBank].state = XMEM_STATE_RUNNING;
                interrupts();
        }

        /**
         *
         * @return bank that is reserved for this task. 0 = failure.
         *
         * Allocate an extra bank for this task.
         *
         */
        uint8_t AJK_NI AllocateExtraBank(void) {
                noInterrupts();
                for(int i = 1; i < XMEM_MAX_BANK_HEAPS; i++) {
                        if(i == currentBank) continue; // Don't free or check current task!
                        if(tasks[i].state == XMEM_STATE_DEAD) tasks[i].state = XMEM_STATE_FREE; // collect a dead task.
                        if(tasks[i].state == XMEM_STATE_FREE) {
                                tasks[i].state = XMEM_STATE_USED; //
                                tasks[i].parent = currentBank; // parent
                                interrupts();
                                return i;
                        }
                }
                interrupts();
                return 0;
        }

        /**
         *
         * @param bank memory bank to free or task to explicitly kill
         * @return nothing
         *
         * Free an allocated bank or kill a task.
         * Bank or task must be owned by parent task.
         */
        void AJK_NI FreeBank(uint8_t bank) {
                noInterrupts();
                if(currentBank == tasks[bank].parent) {
                        tasks[bank].state = XMEM_STATE_FREE;
                }
                interrupts();
        }

        /**
         *
         * @param p memory copier pipe in AVR RAM
         * @return status
         */
        bool AJK_NI memory_ready(memory_stream *p) {
                noInterrupts();
                bool rv = p->ready;
                interrupts();
                return rv;
        }

        /**
         *
         * @param data data to copy
         * @param length length of the data
         * @param p memory copier pipe in AVR RAM
         */
        void AJK_NI memory_send(uint8_t *data, uint16_t length, memory_stream *p) {
                while(p->ready) Yield(); // Since multiple senders are possible, wait.
                noInterrupts();
                p->data = data;
                p->data_len = length;
                p->bank = currentBank;
                p->ready = true;
                interrupts();
                while(p->ready) Yield(); // Wait here, because caller may free right after!
        }

        void * AJK_NI safe_malloc(size_t x) {
                void *data = malloc(x);
                if(data == NULL) {
                        Serial.write("\r\n\r\nOOM LEN=");
                        Serial.println(x);
                        Serial.write("PID=");
                        Serial.println(currentBank);
                        Serial.flush();
                        for(;;);
                }
                return data;
        }


        // byte based pipes, much like un*x

        /**
         *
         * @param c 8bit data
         * @param p pipe in AVR RAM
         */
        void AJK_NI pipe_put(uint8_t c, pipe_stream *p) {
                while(p->ready) Yield();
                p->data = c;
                p->ready = true;
        }

        /**
         *
         * @param p pipe in AVR RAM
         * @return 8bit data
         */
        uint8_t AJK_NI pipe_get(pipe_stream *p) {
                uint8_t c;
                while(!p->ready) Yield();
                c = p->data;
                p->ready = false;
                return c;
        }

        /**
         *
         * @param message message to send
         * @param len length of message
         * @param p pipe to send message on in AVR RAM
         */
        void AJK_NI pipe_send_message(uint8_t *message, int len, pipe_stream *p) {
                pipe_put(len & 0xff, p);
                pipe_put((len >> 8) & 0xff, p);
                while(len) {
                        pipe_put(*message, p);
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
        int AJK_NI pipe_recv_message(uint8_t **message, pipe_stream *p) {
                int len;
                int rv;
                len = pipe_get(p);
                len += pipe_get(p) << 8;
                rv = len;
                *message = (uint8_t *)safe_malloc(len);
                uint8_t *ptr = *message;
                while(len) {
                        *ptr = pipe_get(p);
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
        bool AJK_NI pipe_ready(pipe_stream *p) {
                noInterrupts();
                bool rv = p->ready;
                interrupts();
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
        void AJK_NI pipe_init(pipe_stream *p) {
                p->ready = false;
        }

        /**
         * Copy a block of RAM to another tasks arena or acquired bank.
         *
         * @param d destination address
         * @param s source address
         * @param length length to copy
         * @param db destination task
         */
        void AJK_NI copy_to_task(void *d, void *s, uint16_t length, uint8_t db) {
                SoftCLI();
                noInterrupts();
                register uint8_t mb = currentBank;
                register uint8_t ob = db;
                register char *ss = (char *)s;
                register char *dd = (char *)d;
                register unsigned int csp = SP;
                interrupts();
                while(length) {
                        register uint16_t l = length;
                        if(l > _RAM_COPY_SZ) l = _RAM_COPY_SZ;
                        memcpy(cpybuf, ss, l);
                        noInterrupts();
                        SP = keepstack; // original on-chip ram
                        flipBank(ob);
                        interrupts();
                        memcpy(dd, cpybuf, l);
                        noInterrupts();
                        flipBank(mb);
                        SP = csp;
                        interrupts();
                        length -= l;
                        ss += l;
                        dd += l;
                }
                SoftSEI();
        }

        /**
         * Copy block of memory from another tasks arena.
         *
         * @param d destination address
         * @param s source address
         * @param length length to copy
         * @param sb source task
         */
        void AJK_NI copy_from_task(void *d, void *s, uint16_t length, uint8_t sb) {
                SoftCLI();
                noInterrupts();
                register uint8_t mb = currentBank;
                register uint8_t ob = sb;
                register char *ss = (char *)s;
                register char *dd = (char *)d;
                register unsigned int csp = SP;
                interrupts();
                while(length) {
                        register uint16_t l = length;
                        if(l > _RAM_COPY_SZ) l = _RAM_COPY_SZ;
                        noInterrupts();
                        SP = keepstack;
                        flipBank(ob);
                        interrupts();
                        memcpy(cpybuf, ss, l);
                        noInterrupts();
                        flipBank(mb);
                        SP = csp;
                        interrupts();
                        memcpy(dd, cpybuf, l);
                        length -= l;
                        ss += l;
                        dd += l;
                }
                SoftSEI();
        }

        /**
         *
         * @param data pointer to be allocated
         * @param p memory copier pipe in AVR RAM
         * @return length of message
         */
        uint16_t AJK_NI memory_recv(uint8_t **data, memory_stream *p) {
                while(!p->ready) Yield();
                *data = (uint8_t *)safe_malloc(p->data_len);
                copy_from_task((void *)(*data), (void *)(p->data), p->data_len, p->bank);
                noInterrupts();
                register uint16_t length = p->data_len;
                p->ready = false;
                interrupts();
                return length;
        }

        /**
         * @return which task started this task.
         */
        uint8_t AJK_NI Task_Parent() {
                return tasks[currentBank].parent;
        }

        /**
         *
         * @param which task ID
         * @return is the child or parent process is running.
         *
         * Only the parent or child process should call this.
         */
        bool AJK_NI Is_Running(uint8_t which) {
                if((tasks[which].parent != currentBank) || (tasks[currentBank].parent != which)) return false;
                return (tasks[which].state == XMEM_STATE_RUNNING || tasks[which].state == XMEM_STATE_SLEEP);
        }

        /**
         *
         * @param which task ID
         * @return is the child or parent process is done.
         *
         * Only the parent or child process should call this.
         *
         */
        bool AJK_NI Is_Done(uint8_t which) {
                if((tasks[which].parent != currentBank) || (tasks[currentBank].parent != which)) return true;
                return (tasks[which].state == XMEM_STATE_DEAD);
        }

        /**
         *
         * @param which task ID
         * @return state of this task.
         */
        uint8_t AJK_NI Task_State(uint8_t which) {
                return tasks[currentBank].state;
        }

        /**
         *
         * @param which task ID
         * @return task is a child of calling task.
         *
         */
        bool AJK_NI Task_Is_Mine(uint8_t which) {
                return (tasks[which].parent == currentBank);
        }

        /**
         *
         * @param object Caution, must be in AVR RAM
         */
        void AJK_NI Lock_Acquire(volatile uint8_t *object) {
retry_lock:
                noInterrupts();
                if(*object != 0) {
                        interrupts();
                        Yield();
                        goto retry_lock;
                }
                *object = 1;
                interrupts();
        }

        /**
         *
         * @param object Caution, must be in AVR RAM
         */
        void AJK_NI Lock_Release(volatile uint8_t *object) {
                noInterrupts();
                *object = 0;
                interrupts();
        }

        /**
         * @param sleep how long to sleep for in milliseconds.
         *
         * 10us setting:
         * Warning, using this setting with Yield/Sleep can actually HURT performance!
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
         * 100us setting, this is the default:
         * do the math, roughly 2x the 50us delay, half the resolution of 50us. Duh ;-)
         *
         */
        void AJK_NI Sleep(uint64_t sleep) {
                if(sleep == 0) return; // what? :-)
                noInterrupts();
#ifdef hundredus
                tasks[currentBank].sleep = sleep;
#else
#ifdef fiftyus
                tasks[currentBank].sleep = sleep * 2;
#else
                tasks[currentBank].sleep = sleep * 10;
#endif
#endif
                tasks[currentBank].state = XMEM_STATE_SLEEP;
                interrupts();
                while(tasks[currentBank].state == XMEM_STATE_SLEEP) Yield();
        }

        /**
         *
         * @param which Task ID to start or continue after pausing, wake sleep.
         *
         * Do nothing if task is not a child of this parent, or the parent of this process
         *
         */
        void AJK_NI StartTask(uint8_t which) {
                if(tasks[currentBank].parent != which && tasks[which].parent != currentBank) return;
                noInterrupts();
                if(tasks[which].state == XMEM_STATE_PAUSED) tasks[which].state = XMEM_STATE_RUNNING; // running
                else if(tasks[which].state == XMEM_STATE_SLEEP) tasks[which].state = XMEM_STATE_RUNNING; // running
                interrupts();
        }

        /**
         *
         * @param which Task ID to pause
         *
         * Do nothing if task is not a child of this parent, or the parent of this process
         *
         */
        void AJK_NI PauseTask(uint8_t which) {
                if(tasks[currentBank].parent != which && tasks[which].parent != currentBank) return;
                while(tasks[which].state == XMEM_STATE_SLEEP) Yield();
                noInterrupts();
                if(tasks[which].state == XMEM_STATE_RUNNING) tasks[which].state = XMEM_STATE_PAUSED; // setup/pause state
                interrupts();
        }

        /**
         * This is called when a task has exited.
         */
        void AJK_NI TaskFinish(void) {
                Serial.write("Task ended");
                noInterrupts();
                tasks[currentBank].state = XMEM_STATE_DEAD; // dead
                interrupts();
                Yield();
                // should never come here
                for(;;);
        }

        uint8_t AJK_NI SetupTask(void (*func)(void), unsigned int ofs) {
                // r25:r24 contains func on 128x,
                // r25:24:r22 for 256x?
                cli();
                asm volatile ("push r24"); // lo8
                asm volatile ("pop r4");
                asm volatile ("push r25"); // hi 8
                asm volatile ("pop r5");

                //#if defined (__AVR_ATmega2560__)
                //        asm volatile ("push r22"); // loh8
                //        asm volatile ("pop r3");
                //
                //#endif
                if(ofs > 30719 || ofs < 1024) {
                        sei();
                        return 0; // FAIL!
                }
                register unsigned int oldsp;
                register uint8_t oldbank;
                register int i;
                for(i = 1; i < XMEM_MAX_BANK_HEAPS; i++) {
                        if(i == currentBank) continue; // Don't free or check current task!
                        if(tasks[i].state == XMEM_STATE_DEAD) tasks[i].state = XMEM_STATE_FREE; // collect a dead task.
                        if(tasks[i].state == XMEM_STATE_FREE) {
                                tasks[i].state = XMEM_STATE_PAUSED; // setting up/paused
                                tasks[i].parent = currentBank; // parent
                                tasks[i].sp = (0x7FFF + ofs);
                                // (re)-set the arena pointers
                                bankHeapStates[i].__malloc_heap_start = (char *)tasks[i].sp + 1;
                                bankHeapStates[i].__brkval = bankHeapStates[i].__malloc_heap_start;
                                bankHeapStates[i].__flp = NULL;
                                // switch banks, and do setup
                                oldbank = currentBank;
                                oldsp = SP;
                                SP = keepstack; // original on-chip ram
                                setMemoryBank(i, false);
                                SP = tasks[i].sp;
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
                                asm volatile("eor r3,r3");
                                asm volatile ("push r3"); // hh8
#endif
                                // fill in all registers.
                                // I save all because I may want to do tricks later on.
                                asm volatile ("push r0");
                                asm volatile ("in r0, __SREG__");
                                asm volatile ("push r0");
                                asm volatile ("in r0 , 0x3b");
                                asm volatile ("push r0");
                                asm volatile ("in r0 , 0x3c");
                                asm volatile ("push r0");
                                asm volatile ("push r1");
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

        /**
         *
         * @param func function that becomes a new task.
         * @return Task ID
         *
         * Uses the defaults from xmem.h, 8192 bytes
         */
        uint8_t AJK_NI SetupTask(void (*func)(void)) {
                return SetupTask(func, (EXT_RAM_STACK_ARENA));
        }
        // Sleep timer ISR + context switch trigger

        //        ISR(TIMER3_COMPA_vect, ISR_BLOCK) {

        ISR(TIMER3_COMPA_vect, ISR_NAKED) {
                asm volatile ("push r0");
                asm volatile ("in r0, __SREG__");
                asm volatile ("push r0");
                asm volatile ("cli");
                asm volatile ("in r0 , 0x3b");
                asm volatile ("push r0");
                asm volatile ("in r0 , 0x3c");
                asm volatile ("push r0");
                asm volatile ("push r1");
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

                TCNT3 = 0;

                register uint8_t check;

                for(check = 0; check < XMEM_MAX_BANK_HEAPS; check++) {
                        if(tasks[check].state == XMEM_STATE_SLEEP) {
                                if(tasks[check].sleep == 0llu) {
                                        tasks[check].state = XMEM_STATE_RUNNING;
                                } else {
                                        tasks[check].sleep--;
                                }
                        }
                }
                TCNT3 = 0;

                if(tasks[currentBank].state == XMEM_STATE_YIELD) tasks[currentBank].state = XMEM_STATE_RUNNING;
                if(tasks[currentBank].state != XMEM_STATE_HOG_CPU) {

                        check = currentBank;
                        do {
                                check++;
                                if(check == XMEM_MAX_BANK_HEAPS) check = 0;
                                if(tasks[check].state == XMEM_STATE_RUNNING) break;
                        } while(check != currentBank);

                        if(check != currentBank) { // skip if we are not context switching
                                tasks[currentBank].sp = SP;
                                SP = keepstack; // original on-chip ram
                                setMemoryBank(check, true);
                                SP = tasks[currentBank].sp;
                                //digitalWrite(LED_BUILTIN, (currentBank & 1) ? HIGH : LOW);
                        }
                }
                TCNT3 = 0;

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
                asm volatile ("pop r1");
                asm volatile ("pop r0");
                asm volatile ("out 0x3c , r0");
                asm volatile ("pop r0");
                asm volatile ("out 0x3b , r0");
                asm volatile ("pop r0");
                asm volatile ("out __SREG__ , r0");
                asm volatile ("pop r0");
                asm volatile ("reti");
        }

        void AJK_NI MultitaskBegin(void) {
                noInterrupts();
                //attachInterrupt(6, Context_ISR, CHANGE);
                //DDRE |= _BV(PE6);
                // Set up task switching clock
                TCCR3A = 0;
                TCCR3B = 0;
                OCR3A = CLK_CMP;
                TCCR3B |= CLK_prescale8;
                TCNT3 = 0;
                TIMSK3 |= (1 << OCIE3A); // Enable timer
                interrupts();
        }

#else

#if WANT_TEST_CODE

        /*
         * Self test the memory. This will destroy the entire content of all
         * memory banks so don't use it except in a test scenario.
         */

        SelfTestResults AJK_NI selfTest() {
                SelfTestResults results;
                if(totalBanks == 0) {
                        results.succeeded = false;
                        results.failedBank = 0;
                        results.failedAddress = reinterpret_cast<uint8_t *>(0x2200);
                        return results;
                }
#if EXT_RAM
                volatile uint8_t *ptr;
                uint8_t bank, writeValue, readValue;
                // write an ascending sequence of 1..237 running through
                // all memory banks
                writeValue = 1;
                for(bank = 0; bank < totalBanks; bank++) {

                        setMemoryBank(bank);

                        for(ptr = reinterpret_cast<uint8_t *>(0xFFFF); ptr >= reinterpret_cast<uint8_t *>(0x2200); ptr--) {
                                *ptr = writeValue;

                                if(writeValue++ == 237)
                                        writeValue = 1;
                        }
                }

                // verify the writes

                writeValue = 1;
                for(bank = 0; bank < 8; bank++) {

                        setMemoryBank(bank);

                        for(ptr = reinterpret_cast<uint8_t *>(0xFFFF); ptr >= reinterpret_cast<uint8_t *>(0x2200); ptr--) {

                                readValue = *ptr;

                                if(readValue != writeValue) {
                                        results.succeeded = false;
                                        results.failedAddress = ptr;
                                        results.failedBank = bank;
                                        return results;
                                }

                                if(writeValue++ == 237)
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


#endif

#endif


#if 0

uint8_t SetupTask(void (*func)(void), unsigned int ofs) {
        // r25:r24 contains func on 128x,
        // r25:24:r22 for 256x?
        cli();
        asm volatile ("push r24"); // lo8
        asm volatile ("pop r4");
        asm volatile ("push r25"); // hi 8
        asm volatile ("pop r5");
#if defined (__AVR_ATmega2560__)
        asm volatile ("push r22"); // loh8
        asm volatile ("pop r3");
#endif
        if(ofs > 30719 || ofs < 1024) {
                sei();
                return 0; // FAIL!
        }
        register unsigned int oldsp;
        register uint8_t oldbank;
        register int i;
        for(i = 1; i < XMEM_MAX_BANK_HEAPS; i++) {
                if(i == currentBank) continue; // Don't free or check current task!
                if(tasks[i].state == XMEM_STATE_DEAD) tasks[i].state = XMEM_STATE_FREE; // collect a dead task.
                if(tasks[i].state == XMEM_STATE_FREE) {
                        tasks[i].state = XMEM_STATE_PAUSED; // setting up/paused
                        tasks[i].parent = currentBank; // parent
                        tasks[i].sp = (0x7FFF + ofs);
                        // (re)-set the arena pointers
                        bankHeapStates[i].__malloc_heap_start = (char *)tasks[i].sp + 1;
                        bankHeapStates[i].__brkval = bankHeapStates[i].__malloc_heap_start;
                        bankHeapStates[i].__flp = NULL;
                        // switch banks, and do setup
                        oldbank = currentBank;
                        oldsp = SP;
                        SP = keepstack; // original on-chip ram
                        setMemoryBank(i, false);
                        SP = tasks[i].sp;
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
                        asm volatile ("push r0");
                        asm volatile ("in r0, __SREG__");
                        asm volatile ("push r0");
                        asm volatile ("in r0 , 0x3b");
                        asm volatile ("push r0");
                        asm volatile ("in r0 , 0x3c");
                        asm volatile ("push r0");
                        asm volatile ("push r1");
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

// Context switch ISR, do what we need to do as fast as possible.
// We count what we do in here as part of the task time.
// Therefore, actual task time is variable.
// It may seem unfortunate that all registers must be pushed,
// but I like to save the entire context, because you never know what
// custom assembler may be using for registers.

ISR(INT6_vect, ISR_NAKED) {
        asm volatile ("push r0");
        asm volatile ("in r0, __SREG__");
        asm volatile ("push r0");
        asm volatile ("in r0 , 0x3b");
        asm volatile ("push r0");
        asm volatile ("in r0 , 0x3c");
        asm volatile ("push r0");
        asm volatile ("push r1");
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
        if(tasks[currentBank].state == XMEM_STATE_YIELD) tasks[currentBank].state = XMEM_STATE_RUNNING;
        if(tasks[currentBank].state == XMEM_STATE_HOG_CPU) goto flop;
        for(check = currentBank + 1; check < XMEM_MAX_BANK_HEAPS; check++) {
                if(tasks[check].state == XMEM_STATE_RUNNING) goto flip;
        }
        // Check tasks before current.
        for(check = 0; check < currentBank; check++) {
                if(tasks[check].state == XMEM_STATE_RUNNING) break;
        }
flip:
        if(check != currentBank) { // skip if we are not context switching
                tasks[currentBank].sp = SP;
                SP = keepstack; // original on-chip ram
                setMemoryBank(check, true);
                SP = tasks[currentBank].sp;
        }
flop:
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
        asm volatile ("pop r1");
        asm volatile ("pop r0");
        asm volatile ("out 0x3c , r0");
        asm volatile ("pop r0");
        asm volatile ("out 0x3b , r0");
        asm volatile ("pop r0");
        asm volatile ("out __SREG__ , r0");
        asm volatile ("pop r0");
        asm volatile ("reti");
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
uint8_t AJK_NI SetupTask(void (*func)(void), unsigned int ofs) {
        int i = 0;
        if(ofs < 30720 && ofs > 1023) {
#ifdef __AVR_ATmega2560__
                int sks = 28;
#else
                int sks = 25;
#endif
                uint32_t fvec = (uint32_t)func;
                uint32_t fin = (uint32_t)TaskFinish;
                unsigned char newstack[] = {

#ifdef __AVR_ATmega2560__
                        /*
                         * HL
                         */
                        (uint8_t)((isr_6_push & 0x00ff0000LU) >> 16),
#endif
                        /*
                         * LH   LL
                         */
                        (uint8_t)((isr_6_push & 0x0000ff00LU) >> 8),
                        (uint8_t)((isr_6_push & 0x000000ffLU)),
                        /*
                         * Registers
                         * R31  R30  R27  R26   R25   R24   R23   R22   R21   R20   R19   R18
                         */
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        /*
                         * Flags and special Registers
                         * 0x3b 0x3f R0  R1
                         */
                        //0x3b
                        0x00,
                        //0x3f
                        0x00,
                        // R0 (tmp reg)
                        0x00,
                        // R1 (zero reg, MUST be zero!)
                        0x00,

                        /*
                         * task location
                         */
#ifdef __AVR_ATmega2560__
                        /*
                         * HL
                         */
                        (uint8_t)((fvec & 0x00ff0000LU) >> 16),
#endif
                        /*
                         * LH   LL
                         */
                        (uint8_t)((fvec & 0x0000ff00LU) >> 8),
                        (uint8_t)((fvec & 0x000000ffLU)),

                        /*
                         *  TaskFinish
                         */
#ifdef __AVR_ATmega2560__
                        /*
                         * HL
                         */
                        (uint8_t)((fin & 0x00ff0000LU) >> 16),
#endif
                        /*
                         * LH   LL
                         */
                        (uint8_t)((fin & 0x0000ff00LU) >> 8),
                        (uint8_t)((fin & 0x000000ffLU)),

                        /*
                         *  Guard (reset)
                         */
#ifdef __AVR_ATmega2560__
                        /*
                         * HL
                         */
                        0,
#endif
                        /*
                         * LH   LL
                         */
                        0, 0
                        /*
                         *  stack top
                         */
                };

                noInterrupts();

                for(i = 1; i < XMEM_MAX_BANK_HEAPS; i++) {
                        if(i == currentBank) continue; // Don't free or check current task!
                        if(tasks[i].state == XMEM_STATE_DEAD) tasks[i].state = XMEM_STATE_FREE; // collect a dead task.
                        if(tasks[i].state == XMEM_STATE_FREE) {
                                tasks[i].state = XMEM_STATE_PAUSED; // setting up/paused
                                tasks[i].parent = currentBank; // parent
                                tasks[i].sp = ((0x7FFFU + ofs));
                                // (re)-set the arena pointers
                                bankHeapStates[i].__malloc_heap_start = (char *)tasks[i].sp + 1;
                                bankHeapStates[i].__brkval = bankHeapStates[i].__malloc_heap_start;
                                bankHeapStates[i].__flp = NULL;
                                tasks[i].sp -= sks;
                                interrupts();
                                copy_to_task((void *)tasks[i].sp, (void *)newstack, sks, i);
                                //noInterrupts();
                                tasks[i].sp--;
                                break;
                        }
                }
                interrupts();
        }
        if(i == XMEM_MAX_BANK_HEAPS) i = 0;
        return i;
}

        void AJK_NI Context_ISR(void) {
                noInterrupts();
                //digitalWrite(LED_BUILTIN, (PORTE & _BV(PE6)) ? HIGH : LOW );
                if(tasks[currentBank].state == XMEM_STATE_YIELD) tasks[currentBank].state = XMEM_STATE_RUNNING;
                if(tasks[currentBank].state != XMEM_STATE_HOG_CPU) {

                        uint8_t check = currentBank;
                        do {
                                check++;
                                if(check == XMEM_MAX_BANK_HEAPS) check = 0;
                                //if(tasks[check].state == XMEM_STATE_YIELD) tasks[check].state = XMEM_STATE_RUNNING;
                                if(tasks[check].state == XMEM_STATE_RUNNING) break;
                        } while(check != currentBank);

                        if(check != currentBank) { // skip if we are not context switching
                                tasks[currentBank].sp = SP;
                                SP = keepstack; // original on-chip ram
                                setMemoryBank(check, true);
                                SP = tasks[currentBank].sp;
                                digitalWrite(LED_BUILTIN, (currentBank & 1) ? HIGH : LOW);
                        }
                }
                interrupts();
        }

        void AJK_NI Prepare_ISR(void) {
                noInterrupts();
#ifdef __AVR_ATmega2560__
                // uint32_t *x = (uint32_t *)(SP);
                uint16_t x = SP + 1;
                isr_6_push = (*((uint8_t *)(x++)));
                isr_6_push = isr_6_push << 8;
                isr_6_push |= (*((uint8_t *)(x++)));
                isr_6_push = isr_6_push << 8;
                isr_6_push |= (*((uint8_t *)(x++)));
#else
                uint16_t x = SP + 1;
                isr_6_push = (*((uint8_t *)(x++)));
                isr_6_push = isr_6_push << 8;
                isr_6_push |= (*((uint8_t *)(x++)));
#endif
                detachInterrupt(6);
                interrupts();
        }
#endif