#include "ports.h"
#include "interrupts.h"
#include "handlers.h"
#include "video.h"

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
 *
 * See http://wiki.osdev.org/PS/2_Keyboard for details.
 */
#define KEYBOARD_PORT 0x60


/* TODO:  You can create static variables here to hold keyboard state.
 *        Note that if you create some kind of circular queue (a very good
 *        idea, you should declare it "volatile" so that the compiler knows
 *        that it can be changed by exceptional control flow.
 *
 *        Also, don't forget that interrupts can interrupt *any* code,
 *        including code that fetches key data!  If you are manipulating a
 *        shared data structure that is also manipulated from an interrupt
 *        handler, you might want to disable interrupts while you access it,
 *        so that nothing gets mangled...
 */

volatile static int num_presses;


// If the "A" key was pressed, then advance a "KeyboardSaysHi" message
// horizontally across the screen
void handle_keyboard_interrupt(void) {
    unsigned char scan_code = inb(KEYBOARD_PORT);
    if (scan_code == 0x1E) {
        // The "A" key was pressed
        char * message =         "KeyboardSaysHi";
        char * blank_messagage = "              ";
        int prev_offset = (num_presses % 80) + 80 * 10;
        write_string(GREEN, blank_messagage, prev_offset);
        num_presses++;
        int offset = (num_presses % 80) + 80 * 10;
        write_string(GREEN, message, offset);
    }
}

void init_keyboard(void) {
    /* TODO:  Initialize any state required by the keyboard handler. */
    num_presses = 0;

    install_interrupt_handler(KEYBOARD_INTERRUPT, irq_keyboard_handler);
}

