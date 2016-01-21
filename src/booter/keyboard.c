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
 *
 * See http://wiki.osdev.org/PS/2_Keyboard for details.
 */
#define KEYBOARD_PORT 0x60


volatile static int num_presses;


void handle_keyboard_interrupt(void) {
    if (win || win == -1) {
        return;
    }
    unsigned char scan_code = inb(KEYBOARD_PORT);
    if (scan_code == 0x1E) {
        shots[0].y_coord = 0;
    }

    if (scan_code == 0x4B) {
        // The left arrow key was pressed
        write_string(BLACK_ON_CYAN, " ", player.y_coord * 80 + player.x_coord);
        write_string(BLACK_ON_CYAN, " ", (player.y_coord + 1) * 80 + player.x_coord);
        player.x_coord--;
        draw_player();
    }

    if (scan_code == 0x4D) {
        // The right arrow key was pressed
        write_string(BLACK_ON_CYAN, " ", player.y_coord * 80 + player.x_coord);
        write_string(BLACK_ON_CYAN, " ", (player.y_coord + 1) * 80 + player.x_coord);
        player.x_coord++;
        draw_player();
        if (player.x_coord >= WIDTH - 1) {
            win = 1;
        }
    }
}

void init_keyboard(void) {
    num_presses = 0;
    install_interrupt_handler(KEYBOARD_INTERRUPT, irq_keyboard_handler);
}
