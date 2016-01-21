#include "keyboard.h"
#include "ports.h"
#include "interrupts.h"
#include "handlers.h"
#include "video.h"
#include "game.h"

/* This is the IO port of the PS/2 controller, where the keyboard's scan
 * codes are made available.  Scan codes can be read as follows:
 *
 *     unsigned char scan_code = inb(KEYBOARD_PORT);
 *
 * Most keys generate a scan-code when they are pressed, and a second scan-
 * code when the same key is released.  For such keys, the only difference
 * between the "pressed" and "released" scan-codes is that the top bit is
 * cleared in the "pressed" scan-code, and it is set in the "released" scan-
 * code.
 *
 * A few keys generate two scan-codes when they are pressed, and then two
 * more scan-codes when they are released.  For example, the arrow keys (the
 * ones that aren't part of the numeric keypad) will usually generate two
 * scan-codes for press or release.  In these cases, the keyboard controller
 * fires two interrupts, so you don't have to do anything special - the
 * interrupt handler will receive each byte in a separate invocation of the
 * handler.

 * See http://wiki.osdev.org/PS/2_Keyboard for details.
 */
#define KEYBOARD_PORT 0x60


// Displacement of the avatar since last call to get_new_displacement()
volatile static int displacement;

/*
 * Return the current value of |displacement| without letting it get mangled by
 * interrupts, then reset the displacement to 0.
 */
int get_new_displacement(void) {
    int new_displacement;
    disable_interrupts();
    new_displacement = displacement;
    displacement = 0;
    enable_interrupts();
    return new_displacement;
}

/*
 * Handle a keyboard interrupt by modifying the global |displacement| variable
 * appropriately.
 */
void handle_keyboard_interrupt(void) {
    unsigned char scan_code = inb(KEYBOARD_PORT);
    if (scan_code == RIGHT_ARROW) {
        displacement++;
    }
    else if (scan_code == LEFT_ARROW) {
        displacement--;
    }
}

void init_keyboard(void) {
    displacement = 0;
    install_interrupt_handler(KEYBOARD_INTERRUPT, irq_keyboard_handler);
}
