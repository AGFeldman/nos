.globl handle_timer_interrupt
.globl handle_keyboard_interrupt


// Timer handler
.align 4
.globl irq_timer_handler
irq_timer_handler:
        pusha   # Save registers from the interrupted code!
        cld     # If calling C code, clear direction flag

        call handle_timer_interrupt

        # Acknowledge the interrupt so that the PIC will send more
        # interrupts!  We do this by sending 0x20 to port 0x20.
        # See http://wiki.osdev.org/8259_PIC#End_of_Interrupt for
        # details.
        mov     $0x20, %al
        out     %al, $0x20

        popa    # Restore all registers
        iret    # Go back to interrupted code


// Keyboard handler
.align 4
.globl irq_keyboard_handler
irq_keyboard_handler:
        pusha   # Save registers from the interrupted code!
        cld     # If calling C code, clear direction flag

        call handle_keyboard_interrupt

        # Acknowledge the interrupt so that the PIC will send more
        # interrupts!  We do this by sending 0x20 to port 0x20.
        # See http://wiki.osdev.org/8259_PIC#End_of_Interrupt for
        # details.
        mov     $0x20, %al
        out     %al, $0x20

        popa    # Restore all registers
        iret    # Go back to interrupted code
