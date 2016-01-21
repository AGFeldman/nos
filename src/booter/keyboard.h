#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEYBOARD_BUFFER_SIZE 20
#define LEFT_ARROW 0x4B
#define RIGHT_ARROW 0x4D

int get_new_displacement(void);

void handle_keyboard_interrupt(void);

void init_keyboard(void);

#endif /* KEYBOARD_H */

