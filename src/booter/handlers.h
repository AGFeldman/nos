#ifndef HANDLERS_H
#define HANDLERS_H


// TODO(agf): Remove this comment block
/* Declare the entry-points to the interrupt handler assembly-code fragments,
 * so that the C compiler will be happy.
 *
 * You will need lines like these:  void *(irqN_handler)(void)
 */

void *(irq_timer_handler)(void);

void *(irq_keyboard_handler)(void);


#endif /* HANDLERS_H */
