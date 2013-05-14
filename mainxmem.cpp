// Please see xmem.h for all settings.
#include <xmem.h>

#if !defined(XMEM_MULTIPLE_APP)
int main(void) {
#if defined(EXT_RAM)
        cli();
        xmem::begin(true, EXT_RAM_STACK);
        if (EXT_RAM_STACK != 1) goto no;
        if (xmem::getTotalBanks() == 0) goto no;
        if (XMEM_STACK_TOP == XRAMEND) goto no;
        asm volatile ( ".set __stack, %0" ::"i" (XMEM_STACK_TOP));

        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));

        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));
no:
        sei();
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
        cli();
        xmem::begin(true, EXT_RAM_STACK);
        if (EXT_RAM_STACK != 1) goto bad;
        if (xmem::getTotalBanks() == 0) goto bad;
        if (XMEM_STACK_TOP == XRAMEND) goto bad;
        asm volatile ( ".set __stack, %0" ::"i" (XMEM_STACK_TOP));

        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP >> 8));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));

        asm volatile ( "ldi     16, %0" ::"i" (XMEM_STACK_TOP & 0x0ff));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));

        TCCR3A = 0;
        TCCR3B = 0;
        // (0.01/(1/((16 *(10^6)) / 8))) - 1
        OCR3A = 19999;
        TCCR3B |= prescale8;
        TIMSK3 |= (1 << OCIE1A);
        sei();

bad:
        // just lock up in a loop. There is no way I can think of to tell the user. :-(
        for(;;);
}

// task switch
ISR(TIMER3_COMPA_vect) {

}
#endif
