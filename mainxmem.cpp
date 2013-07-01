// Please see xmem.h for all settings.
#include <xmem.h>

#if !defined(XMEM_MULTIPLE_APP)

int main(void) {
#if defined(EXT_RAM)
        xmem::begin(EXT_RAM_HEAP, EXT_RAM_STACK);
#if EXT_RAM_STACK
        if (xmem::getTotalBanks() == 0) goto no;
        if (XMEM_STACK_TOP == XRAMEND) goto no;
        cli();
        //asm volatile ( ".set __stack, %0" ::"i" (XMEM_STACK_TOP));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));
        sei();
#endif
no:
#endif
        init();
#if defined(USBCON)
        USBDevice.attach();
#endif

        setup();

        for (;;) {
                loop();
                if (serialEventRun) serialEventRun();
        }

        return 0;
}
#else
#if !defined(EXT_RAM)
#error You must enable EXT_RAM.
#endif

int main(void) {
        //if (SP > XRAMEND) goto forever; // crash loop prevention.
        xmem::begin(true, true);
        if (xmem::getTotalBanks() == 0) goto bad;
        if (XMEM_STACK_TOP == XRAMEND) goto bad;
        cli();
        keepstack = SP;
        //asm volatile ( ".set __stack, %0" ::"i" (XMEM_STACK_TOP));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));
        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));
        sei();
        init();
#if defined(USBCON)
        USBDevice.attach();
#endif

        setup();
        // Set up task switching
        cli();
        TCCR3A = 0;
        TCCR3B = 0;
        OCR3A = CLK_CMP;
        TCCR3B |= CLK_prescale8;
        TCNT3 = 0;
        TIMSK3 |= (1 << OCIE3A); // Enable timer
        sei();
forever:

        for (;;) {
                loop();
                if (serialEventRun) serialEventRun();
        }

        return 0;

bad:
        // just lock up in a loop. There is no way I can think of to tell the user. :-(
        cli();
        for (;;);
}



#endif
