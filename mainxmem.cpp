#if defined(__AVR__)
// Please see xmem.h for all settings.
#include <xmem.h>

#if !defined(XMEM_MULTIPLE_APP)

int main(void) {

#if defined(CORE_TEENSY)
        _init_Teensyduino_internal_();
#else
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
        cli();
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));
        sei();
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
        cli();
        // Save the original stack pointer, needed to switch banks.
        keepstack = SP;
        sei();

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
        cli();
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out     %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));

        // Enable ability to trigger IRQ from software.
        DDRE |= _BV(PE6);
        EICRB |= (1 << ISC60);
        EIMSK |= (1 << INT6);
        sei();

        setup();
        cli();
        // Set up task switching
        TCCR3A = 0;
        TCCR3B = 0;
        OCR3A = CLK_CMP;
        TCCR3B |= CLK_prescale8;
        TCNT3 = 0;
        TIMSK3 |= (1 << OCIE3A); // Enable timer
        sei();
forever:

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
        cli();
        for(;;);
}

#endif
#endif
