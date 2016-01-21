#ifndef HANDLERS_H
#define HANDLERS_H


/* Entry-points to the interrupt handler assembly-code fragments */

void *(irq_timer_handler)(void);

void *(irq_keyboard_handler)(void);


#endif /* HANDLERS_H */
