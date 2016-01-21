#include "video.h"

/* This is the address of the VGA text-mode video buffer.  Note that this
 * buffer actually holds 8 pages of text, but only the first page (page 0)
 * will be displayed.
 *
 * Individual characters in text-mode VGA are represented as two adjacent
 * bytes:
 *     Byte 0 = the character value
 *     Byte 1 = the color of the character:  the high nibble is the background
 *              color, and the low nibble is the foreground color
 *
 * See http://wiki.osdev.org/Printing_to_Screen for more details.
 *
 * Also, if you decide to use a graphical video mode, the active video buffer
 * may reside at another address, and the data will definitely be in another
 * format.  It's a complicated topic.  If you are really intent on learning
 * more about this topic, go to http://wiki.osdev.org/Main_Page and look at
 * the VGA links in the "Video" section.
 */
#define VIDEO_BUFFER ((void *) 0xB8000)
#define SCREEN_WIDTH 80  // Experimentally determined. Should look up docs

/* TODO:  You can create static variables here to hold video display state,
 *        such as the current foreground and background color, a cursor
 *        position, or any other details you might want to keep track of!
 */


void clear_screen()
{
    // Clear an 80x80 square (should be more than an entire screen)
    int i;
    for (i = 0; i < 2 * SCREEN_WIDTH * SCREEN_WIDTH; i++)
    {
        *((char *)(VIDEO_BUFFER + i)) = 0;
    }
}

void init_video() 
{
    clear_screen();  
}
void write_string(char str_color, char back_color, const char * letters, int x, int y)
{
    /* Now, we cam write to a string starting at x,y. Note that this will wrap around
     if the string is too long. */ 
    int i = 0;
    char temp =letters[0];
    while(temp != '\0')
    {
        write_char(str_color, back_color, temp, x+i, y);
        i ++;
        temp = letters[i];
    }
}
void write_char(char char_color, char back_color, const char letter,int x, int y)
{
    int pos = x + y * SCREEN_WIDTH;
    int color = back_color << 4;
    color += char_color;
    *((char *)VIDEO_BUFFER + 2*pos) = letter;
    *((char *)VIDEO_BUFFER + 2*pos + 1) = color;
}
void write_int(char char_color, char back_color, int to_print, int x, int y, int length)
{
    int i = 0;
    int size = 0;
    int temp = to_print;
    while(temp > 0)
	{
	temp /= 10;
        size ++;}
    write_char(RED, BLACK, size + '0', 40, 0);
    while(i < size)
    {
        write_char(char_color, back_color, (to_print % 10)+'0', x - i, y);
        to_print /= 10; 
        i ++;
    }
    while(i < length)
    {
	write_char(back_color, back_color, '_', x-i,y);
	i ++;
    }
}
