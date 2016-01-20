/* This is the entry-point for the game! */
#include "video.h"

void c_start(void) {
    /* TODO:  You will need to initialize various subsystems here.  This
     *        would include the interrupt handling mechanism, and the various
     *        systems that use interrupts.  Once this is done, you can call
     *        enable_interrupts() to start interrupt handling, and go on to
     *        do whatever else you decide to do!
     */

    /* Loop forever, so that we don't fall back into the bootloader code. */
    unsigned char c = 'a';
    while (1) 
    {
        write_string(RED, "Hello World!");
    }
}

