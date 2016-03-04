/*
The frame table contains one entry for each frame that contains a user page. Each 
entry in the frame table contains a pointer to the page, if any, that currently 
occupies it, and other data of your choice. The frame table allows Pintos to efficiently 
implement an eviction policy, by choosing a page to evict when no frames are free.
The frames used for user pages should be obtained from the "user pool," by calling 
palloc_get_page(PAL_USER). You must use PAL_USER to avoid allocating from the "kernel 
pool," which could cause some test cases to fail unexpectedly. 
The most important operation on the frame table is obtaining an unused frame. This is 
easy when a frame is free. When none is free, a frame must be made free by evicting 
some page from its frame.
If no frame can be evicted without allocating a swap slot, but swap is full, panic the 
kernel. Real OSes apply a wide range of policies to recover from or prevent such situations, 
but these policies are beyond the scope of this project.
The process of eviction comprises roughly the following steps:
Choose a frame to evict, using your page replacement algorithm. The "accessed" and "dirty" 
bits in the page table, described below, will come in handy.
Remove references to the frame from any page table that refers to it.
Unless you have implemented sharing, only a single page should refer to a frame at any given time.

If necessary, write the page to the file system or to swap.
The evicted frame may then be used to store a different page.
*/

#include <debug.h>
#include <stdint.h>
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "frame.h"

// This is ripped from the calculation in the initialization in palloc.c 
// (1024 comes from hacky gdb experimentation top figure out init_ram_pages)
#define NUM_FRAMES (((1024 * PGSIZE - 1024 * 1024) / PGSIZE) / 2)

typedef struct frame_entry
{
    uintptr_t frame_ptr; // Point to the physical address of the frame
    void *page_table; // The page table with a reference to this frame.
} frame_entry;

// IAMA frame table. AHAHA delicious static allocation
static frame_entry frame_table[NUM_FRAMES] = {{0}}; // Access only through functions in this file

void init_frame_table()
{
    // Maybe something
}

static int find_available_frame_idx(void)
{
    int i;
    for (i = 0; i < NUM_FRAMES; i++)
    {
        if (frame_table[i].page_table == NULL)
            return i;
    }
    return -1;
}

// For now, this just finds the first frame. In the future it should be smarter
static int find_frame_to_evict(void)
{
    int i;
    for (i = 0; i < NUM_FRAMES; i++)
    {
        if (frame_table[i].page_table != NULL)
            return i;
    }
    return -1;
}

// Take care of 
static void clean_up_frame(frame_entry frame)
{
    // umm... what the hell?
}

static void evict_page(void)
{
    PANIC("nuh uh uh. You forgot the magic word");
    bool failed = false;

    // try to do some stuff
    int evict_idx = find_frame_to_evict();
    clean_up_frame(frame_table[evict_idx]);
    palloc_free_page(ptov(frame_table[evict_idx].frame_ptr));
    frame_table[evict_idx].frame_ptr = 0;
    frame_table[evict_idx].page_table = NULL;
    
    // TODO swapping something or something

    if (failed)
    {
        PANIC("Couldn't evict page");
    }
}

// Returns a pointer to the physical address of a new frame
static uintptr_t get_unused_frame(void *page_table)
{
    // This shouldn't actually be necessary if NUM_FRAMES is what I think
    // it should be, but it doesn't hurt to be careful
    if (find_available_frame_idx() == -1)
    {
        evict_page();
    }
    void *new_page = palloc_get_page(PAL_USER | PAL_ZERO);
    if (new_page == NULL) // No pages available, gotta evict
    {
        evict_page();
        new_page = palloc_get_page(PAL_USER | PAL_ZERO);
        if (new_page == NULL) // If it's still mad for some reason even after evicting
            PANIC("Couldn't get a new frame");
    }

    // Have a new page/frame, stick it into the frame table
    int i = find_available_frame_idx();
    frame_table[i].frame_ptr = vtop(new_page);
    frame_table[i].page_table = page_table;

    return vtop(new_page); // vtop translates kernel virtual memory to physical memory
}   