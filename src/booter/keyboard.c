#include "keyboard.h"
#include "video.h"
#include "ports.h"
#include "interrupts.h"
#include "handlers.h"

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
#define QUEUE_SIZE 100

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
    unsigned char *head;
    unsigned char *tail;
    int curr_entries;
    int max_entries;

} circular_queue;

unsigned char queue_array[QUEUE_SIZE]; // Yay static allocation
circular_queue queue = {queue_array, queue_array, 0, QUEUE_SIZE};

// Incrementing for pointers in the circular queue
void circular_ptr_inc(unsigned char **ptr)
{
    *ptr += (*ptr == queue_array + QUEUE_SIZE - 1) ? 1 - QUEUE_SIZE : 1;
}

/*** We should disable interrupts while using the queue, but for now doing that seems
     to just make the program crash ***/

// We drop the newest keystrokes in favor of keeping the oldest
// (not that it matters. this is chess, not speed typing)
void push_queue(unsigned char payload)
{
    disable_interrupts();
    if (queue.curr_entries == queue.max_entries)
    {
        return;
    }
    *(queue.tail) = payload;
    circular_ptr_inc(&(queue.tail));
    queue.curr_entries++;
    enable_interrupts();
}

unsigned char pop_queue()
{   
    disable_interrupts();
    if (queue.head == queue.tail)
    {
        enable_interrupts();
        return '\0'; 
    }
    unsigned char result = *(queue.head);
    circular_ptr_inc(&(queue.head));
    queue.curr_entries--;
    enable_interrupts();
    return result;
}

void init_keyboard(void) 
{
    outb(KEYBOARD_PORT, 0xF2); // This let's us identify the keyboard. Probably isn't necessary
    install_interrupt_handler(0x21, irq1_handler);
}

unsigned char key_map(unsigned int scancode)
{
    switch(scancode)
    {
        case 0x02:
            return '1';
        case 0x03:
            return '2';
        case 0x04:
            return '3';
        case 0x05:
            return '4';
        case 0x06:
            return '5';
        case 0x07:
            return '6';
        case 0x08:
            return '7';
        case 0x09:
            return '8';
        case 0x0A:
            return '9';
        case 0x0B:
            return '0';
        case 0x13:
            return 'r';
        case 0x32:
            return 'm';
        case 0x1F:
            return 's';
        case 0x1C:
            return '\n'; // return carriage
        case 0x0E:
            return '\b'; // Backspace
        default: 
            return '\0';
    }
}

void handle_key_interrupt()
{
    unsigned int key = key_map(inb(KEYBOARD_PORT));
    if (key != '\0')
        push_queue(key);
}
