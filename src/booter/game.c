/* This is the entry-point for the game! */
#define VIDEO_BUFFER ((void *) 0xB8000)

void c_start(void) {
    /* TODO:  You will need to initialize various subsystems here.  This
     *        would include the interrupt handling mechanism, and the various
     *        systems that use interrupts.  Once this is done, you can call
     *        enable_interrupts() to start interrupt handling, and go on to
     *        do whatever else you decide to do!
     */

    /* Loop forever, so that we don't fall back into the bootloader code. */
    unsigned char * vga = (unsigned char *) 0xB8000;
    unsigned char c = 'a';
    int i;
    for (i = 0; i < 1024; i++, c++)
    {
         *(char*)vga = c;
         *(char*)(++vga) = (char)10;
    }
        while (1) 
    {
    }
}

