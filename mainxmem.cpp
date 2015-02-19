#if defined(__AVR__)
// Please see xmem.h for all settings.

#if defined(XMEM_H) && !defined(XMEM_LOADED)

#if ARDUINO >= 106
int atexit(void (*func)()) { return 0; }

void initVariant() __attribute__((weak));
void initVariant() { }

#endif
#if !defined(XMEM_MULTIPLE_APP)
int main(void) {

#if defined(CORE_TEENSY)
        _init_Teensyduino_internal_();
#else
#if ARDUINO >= 106
        initVariant();
#endif
        init();
#if defined(USBCON)
        USBDevice.attach();
#endif
#endif


#if EXT_RAM
        xmem::begin(EXT_RAM_HEAP, EXT_RAM_STACK);
#if EXT_RAM_STACK
        if(xmem::getTotalBanks() == 0) goto no;
        if(XMEM_STACK_TOP == XRAMEND) goto no;
        noInterrupts();
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));
        interrupts();
#endif
no:
#endif
        setup();

        for(;;) {
                loop();
#if !defined(CORE_TEENSY)
                if(serialEventRun) serialEventRun();
#endif
        }

        return 0;
}
#else
#if !EXT_RAM
#error You must enable EXT_RAM.
#endif
int main(void) {

#if defined(CORE_TEENSY)
        _init_Teensyduino_internal_();
#else
        init();
#if defined(USBCON)
        USBDevice.attach();
#endif
#endif

        xmem::begin(true, true);
        if(xmem::getTotalBanks() == 0) goto bad;
        if(XMEM_STACK_TOP == XRAMEND) goto bad;
        noInterrupts();
        // Save the original stack pointer, needed to switch banks.
        keepstack = SP;
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));
        interrupts();

        setup();
        xmem::MultitaskBegin();

        for(;;) {
                loop();
#if !defined(CORE_TEENSY)
                if(serialEventRun) serialEventRun();
#endif
        }

        return 0;

bad:
        //
        // NO external RAM.
        // Use hardware to quickly blink the LED (pin 13) to alert error.
        //
        DDRB |= _BV(PB7);
        // OC1C
        TCCR1A = _BV(COM1C0);
        TCCR1B = _BV(WGM12) | _BV(CS11);
        OCR1A = 0XDEAD;
        noInterrupts();
        for(;;);
}

#endif
#endif
#endif
