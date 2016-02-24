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

#include <list.h>
#include "threads/vaddr.h"
#include "frame_table.h"

#define frame_entry_of(elem) list_entry(elem, struct frame_entry, elem)

struct frame_entry
{
    uintptr_t *frame_ptr;
    void *page_table; // The page table with a referenceto this frame.
    struct list_elem elem;
}

// IAMA frame table
static struct list frame_table; // Access only through functions in this file

void init_frame_table()
{
    list_init(&frame_table);
}

void evict_page()
{
    bool failed = false;

    // try to do some stuff
    
    if (failed)
    {
        // panic somehow
    }
}

// Returns a pointer to the physical address of a new frame
uintptr_t *get_unused_frame()
{
    void *new_page = palloc_get_page(PAL_USER | PAL_ZERO);
    if (new_page == NULL) // No pages available
    {
        evict_page();
        new_page = palloc_get_page(PAL_USER | PAL_ZERO);
    } 
    return vtop(new_page); // vtop translates kernel virtual memory to physical memory
}

uintptr_t *get_and_register_unused_frame(void *page_table)
{
    // This will add a new frame_entry to the frame_table (malloc?).
    // Potentially should also take care of adding the reference to the frame to the
    // page table since the two of those shouldn't ever really happen seperately, so
    // keeping them togoether here means we don't always have to remember to do both
    return NULL;
}
