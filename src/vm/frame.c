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
#include <stdbool.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "page.h"
#include "frame.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/block.h"

// This is ripped from the calculation in the initialization in palloc.c 
// (1024 comes from hacky gdb experimentation to figure out init_ram_pages)
#define NUM_FRAMES (((1024 * PGSIZE - 1024 * 1024) / PGSIZE) / 2)

typedef struct frame_entry
{
    void *kaddr; // Point to the kernel address of the frame
    struct thread *holding_thread; // The thread that's using this frame 
    void *upage;  // The user page the frame is put into
} frame_entry;

// IAMA frame table.
static frame_entry frame_table[NUM_FRAMES] = {{0}};

static int find_available_frame_idx(void)
{
    int i;
    for (i = 0; i < NUM_FRAMES; i++)
    {
        if (frame_table[i].holding_thread == NULL)
            return i;
    }
    return -1;
}

// For now, this just finds the first frame. In the future it should be smarter
static frame_entry *find_frame_to_evict(void)
{
    int i;
    for (i = 0; i < NUM_FRAMES; i++)
    {
        if (frame_table[i].holding_thread != NULL)
            return &frame_table[i];
    }
    return NULL;
}

// For evicting a page. If the page is dirty, write it out to a file or swap
// slot as appropriate. Use the suppl page entry to figure out what's needed
static bool write_out_frame(frame_entry *frame)
{

    suppl_page *s_page = suppl_page_lookup(&frame->holding_thread->suppl_page_table, 
                                                frame->upage);
    if (pagedir_is_dirty(frame->holding_thread->pagedir, s_page))
    {
        // If it's a file and is not ELF stuff
        if (s_page->file_name != NULL && s_page->file_name[0] != '\0' && s_page->elf_status == not_ELF)
        {
            // Write out to file
            // These commented lines might be helpful in writing to a file
    //        struct file * curr_file = filesys_open(curr_page->file_name);
            // In theory, this could fail, but since we know that such a file
            // is in the frame, we shouldn't have to worry about failures.
    //        file_write(curr_file, ptov(curr_page->paddr), curr_page->bytes_to_read);

            struct file * curr_file = filesys_open(s_page->file_name);
            file_write(curr_file, ptov((uintptr_t) s_page->kaddr), s_page->bytes_to_read);
        }
        // Else swap it
        else
        {
            int i;
            for (i = 0; i < BLOCK_SECTOR_SIZE; i++)
            {
                block_write(block_get_role(BLOCK_ROLE_CNT), i*BLOCK_SECTOR_SIZE, s_page);
            }
        }
    }
    
    return false;
}

// TODO make this function take an argument and then make the eviction policy
//cChoose which who to evict rather than ask the evict policy in here
static void evict_frame(frame_entry *evictee)
{
    PANIC("Not ready for evictions yet");
    // If the page is dirty, try to save it. Panic if it can't be swapped out
    if (! write_out_frame(evictee))
        PANIC("Couldn't evict page");
    // Tell the page directory that the page is gone
    pagedir_clear_page(evictee->holding_thread->pagedir, evictee->upage);
    // Tell palloc the page is gone
    palloc_free_page(evictee->kaddr);
    // Smash the page's entry in the frame table
    evictee->kaddr = 0;
    evictee->holding_thread = NULL;
    evictee->upage = NULL;
}

void evict_thread_frames(struct thread *t)
{
    int i;
    for (i = 0; i < NUM_FRAMES; i++)
    {
        //TODO uncomment when eviction works
        //if (frame_table[i].holding_thread == t)
        //    evict_frame(&frame_table[i]);
    }
}

// Returns a pointer to the physical address of a new frame
void *get_unused_frame(struct thread *holding_thread, void *upage)
{
    // This shouldn't actually be necessary if NUM_FRAMES is what I think
    // it should be, but it doesn't hurt to be careful
    if (find_available_frame_idx() == -1)
    {
        evict_frame(find_frame_to_evict());
    }
    void *new_page = palloc_get_page(PAL_USER);
    if (new_page == NULL) // No pages available, gotta evict
    {
        evict_frame(find_frame_to_evict());
        new_page = palloc_get_page(PAL_USER);
        if (new_page == NULL) // If it's still mad for some reason even after evicting
            PANIC("Couldn't get a new frame");
    }

    // Have a new page/frame, stick it into the frame table
    int i = find_available_frame_idx();
    frame_table[i].kaddr = (void *) new_page;
    frame_table[i].holding_thread = holding_thread;
    frame_table[i].upage = upage;

    return (void *) new_page; // vtop translates kernel virtual memory to physical memory
}
