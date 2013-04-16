// Please see xmem.h for all settings.
#include <xmem.h>

int main(void) {
#if defined(EXT_RAM)
        xmem::begin(true, EXT_RAM_STACK);
        if (EXT_RAM_STACK != 1) goto no;
        if (xmem::getTotalBanks() == 0) goto no;
        asm volatile ( ".set __stack, %0" ::"i" (RAMEND));

        asm volatile ( "ldi     16, %0" ::"i" (RAMEND >> 8));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_HI_ADDR));

        asm volatile ( "ldi     16, %0" ::"i" (RAMEND & 0x0ff));
        asm volatile ( "out %0,16" ::"i" (AVR_STACK_POINTER_LO_ADDR));

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

