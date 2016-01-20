#include "ports.h"

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

typedef struct 
{
    unsigned char payload;
    node * next;
} node;

 typedef struct
{
    node *head;
    node *tail;
    int curr_entries;
    int max_entries;

} circular_queue;

void push_queue(unsigned char payload, circular_queue queue)
{
    if (queue->curr_entries == queue->max_entries)
    {
        queue->head = queue->head->next;
    }
    queue->tail = queue->tail->next;
    queue->tail->payload = payload;
    queue->tail->next = NULL;
    queue->curr_entries += 1;
    if (queue->curr_entries == queue->max_entries)
    {
        queue->tail->next = queue->head;
    }
}

unsigned char pop_queue(circular_queue queue)
{
    unsigned char ans = queue->head->payload;
    queue->head = queue->head->next;
    queue->curr_entries -= 1;
    //do we have to deallocate this memory or something?
    return ans;
}

circular_queue * init_queue()
{
    circular_queue * ans = malloc(sizeof(circular_queue));
    ans->curr_entries = 0;
    ans->max_entries = 100;
    ans->head = malloc(sizeof(node));
    ans->tail = malloc(sizeof(node));
    return ans;
}


void init_keyboard(void) {
    disable_interrupts();
    /* TODO:  Initialize any state required by the keyboard handler. */
    outb(KEYBOARD_PORT, 0xF2) // This let's us identify the keyboard. Probably isn't necessary
    // Probably should initialize a circular queue for the keyboard buffer thingamaboberabob
    circular_queue * keyboard_queue = init_queue();
    /* TODO:  You might want to install your keyboard interrupt handler
     *        here as well.
     */
     // What does that mean?
     while(1)
     {
        enable_interrupts();
        unsigned char scan_code = inb(KEYBOARD_PORT);
        disable_interrupts();
        push_queue(scan_code, keyboard_queue);
        enable_interrupts();
     }
}

