#ifndef KEYBOARD_H
#define KEYBOARD_H

void push_queue(unsigned char payload);
unsigned char pop_queue();
void init_keyboard(void);
void handle_key_interrupt();


#endif /* KEYBOARD_H */

