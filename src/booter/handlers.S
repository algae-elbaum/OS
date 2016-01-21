# TODO:  Put IRQ handlers in this file.  The handlers can do whatever you
#        want, but the recommendation is to keep them very simple; for
#        example, call a C function that handles the interrupt and then
#        returns.
#
#        Recall that interrupts are automatically disabled when the handler
#        is invoked, so handlers don't need to worry about being interrupted
#        by other handlers.  However, keep in mind that a handler can
#        interrupt any other code!  Therefore, be sure to disable interrupts
#        when working with data structures shared between interrupt handlers
#        and other code.
#
#        Handlers generally must follow this pattern:
#

.align 4
.globl irq0_handler  # According to wiki.osdev this is the timer
irq0_handler:        # According to the comments, the timer is irq1. argh
    pusha   # Save registers from the interrupted code!
    cld     # If calling C code, clear direction flag


    # Call c fun
    # No need to save any registers, just call
#    call handle_timer_interrupt


    # Acknowledge the interrupt so that the PIC will send more
    # interrupts!  We do this by sending 0x20 to port 0x20.
    # See http://wiki.osdev.org/8259_PIC#End_of_Interrupt for
    # details.
    mov     $0x20, %al
    out     %al, $0x20

    popa    # Restore all registers
    iret    # Go back to interrupted code


.globl irq1_handler  # According to wiki.osdev this is the keyboard
irq1_handler:
    pusha   # Save registers from the interrupted code!
    cld     # If calling C code, clear direction flag


    # Call c fun
    # No need to save any registers, just call
#    call handle_key_interrupt

    # Acknowledge the interrupt so that the PIC will send more
    # interrupts!  We do this by sending 0x20 to port 0x20.
    # See http://wiki.osdev.org/8259_PIC#End_of_Interrupt for
    # details.
    mov     $0x20, %al
    out     %al, $0x20

    popa    # Restore all registers
    iret    # Go back to interrupted code
