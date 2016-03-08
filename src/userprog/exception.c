#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <hash.h>

/*! Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);

/*! Registers handlers for interrupts that can be caused by user programs.

    In a real Unix-like OS, most of these interrupts would be passed along to
    the user process in the form of signals, as described in [SV-386] 3-24 and
    3-25, but we don't implement signals.  Instead, we'll make them simply kill
    the user process.

    Page faults are an exception.  Here they are treated the same way as other
    exceptions, but this will need to change to implement virtual memory.

    Refer to [IA32-v3a] section 5.15 "Exception and Interrupt Reference" for a
    description of each of these exceptions. */
void exception_init(void) {
    /* These exceptions can be raised explicitly by a user program,
       e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
       we set DPL==3, meaning that user programs are allowed to
       invoke them via these instructions. */
    intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
    intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
    intr_register_int(5, 3, INTR_ON, kill,
                      "#BR BOUND Range Exceeded Exception");

    /* These exceptions have DPL==0, preventing user processes from
       invoking them via the INT instruction.  They can still be
       caused indirectly, e.g. #DE can be caused by dividing by
       0.  */
    intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
    intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
    intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
    intr_register_int(7, 0, INTR_ON, kill,
                      "#NM Device Not Available Exception");
    intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
    intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
    intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
    intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
    intr_register_int(19, 0, INTR_ON, kill,
                      "#XF SIMD Floating-Point Exception");

    /* Most exceptions can be handled with interrupts turned on.
       We need to disable interrupts for page faults because the
       fault address is stored in CR2 and needs to be preserved. */
    intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/*! Prints exception statistics. */
void exception_print_stats(void) {
    printf("Exception: %lld page faults\n", page_fault_cnt);
}

/*! Handler for an exception (probably) caused by a user process. */
static void kill(struct intr_frame *f) {
    /* This interrupt is one (probably) caused by a user process.
       For example, the process might have tried to access unmapped
       virtual memory (a page fault).  For now, we simply kill the
       user process.  Later, we'll want to handle page faults in
       the kernel.  Real Unix-like operating systems pass most
       exceptions back to the process via signals, but we don't
       implement them. */

    /* The interrupt frame's code segment value tells us where the
       exception originated. */
    switch (f->cs) {
    case SEL_UCSEG:
        /* User's code segment, so it's a user exception, as we
           expected.  Kill the user process.  */
        printf("%s: dying due to interrupt %#04x (%s).\n",
               thread_name(), f->vec_no, intr_name(f->vec_no));
        intr_dump_frame(f);
        thread_current()->exit_val = -1;
        thread_exit(); 

    case SEL_KCSEG:
        /* Kernel's code segment, which indicates a kernel bug.
           Kernel code shouldn't throw exceptions.  (Page faults
           may cause kernel exceptions--but they shouldn't arrive
           here.)  Panic the kernel to make the point.  */
        intr_dump_frame(f);
        PANIC("Kernel bug - unexpected interrupt in kernel"); 

    default:
        /* Some other code segment?  Shouldn't happen.  Panic the
           kernel. */
        printf("Interrupt %#04x (%s) in unknown segment %04x\n",
               f->vec_no, intr_name(f->vec_no), f->cs);
        thread_exit();
    }
}

/*! Page fault handler.

    At entry, the address that faulted is in CR2 (Control Register
    2) and information about the fault, formatted as described in
    the PF_* macros in exception.h, is in F's error_code member.  The
    example code here shows how to parse that information.  You
    can find more information about both of these in the
    description of "Interrupt 14--Page Fault Exception (#PF)" in
    [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void page_fault(struct intr_frame *f) {
    bool not_present;  /* True: not-present page, false: writing r/o page. */
    bool write;        /* True: access was write, false: access was read. */
    bool user;         /* True: access by user, false: access by kernel. */
    void *fault_addr;  /* Fault address. */

    /* Obtain faulting address, the virtual address that was accessed to cause
       the fault.  It may point to code or to data.  It is not necessarily the
       address of the instruction that caused the fault (that's f->eip).
       See [IA32-v2a] "MOV--Move to/from Control Registers" and
       [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception (#PF)". */
    asm ("movl %%cr2, %0" : "=r" (fault_addr));

    /* Turn interrupts back on (they were only off so that we could
       be assured of reading CR2 before it changed). */
    intr_enable();

    /* Count page faults. */
    page_fault_cnt++;

    /* Determine cause. */
    not_present = (f->error_code & PF_P) == 0;
    write = (f->error_code & PF_W) != 0;
    user = (f->error_code & PF_U) != 0;

    // 1. Locate the page that faulted in the suppl_page_table
    void *upage = (void *) (((uintptr_t) fault_addr >> PGBITS) << PGBITS); // I could have sworn there was a nicer way of doing this
    lock_acquire(&thread_current()->suppl_lock);
    suppl_page *faulted_page = suppl_page_lookup(&thread_current()->suppl_page_table, upage);
    lock_release(&thread_current()->suppl_lock);

    // Either the page doesn't exist because we're not allowed to use it, or the
    // stack needs extending
    if (faulted_page == NULL)
    {
        // See if we should extend the stack
        if (fault_addr > f->esp - 64 && is_user_vaddr(fault_addr))
        {
            // Extend the stack:
            // Create the supplemental page table entry for it
            faulted_page = new_suppl_page(false, upage);
            // Add the supplemental page to the supplemental page table
            hash_insert(&thread_current()->suppl_page_table, &faulted_page->hash_elem);
        }
        // If it's not a stack extension, destroy the process
        else
            kill(f);
    }

    // Now that we're guaranteed to have a non-null faulted_page, do some checks
    // If it's readonly and we tried to write, then there's a problem
    if (faulted_page->read_only && write)
    {
        kill(f);
    }
    // If a user tried to do it and its meant to be in kernel mode, we have a problem.
    if(user && ! is_user_vaddr(fault_addr))
    {
        kill(f);
    }

    // Now get a frame, tie it to the faulting page, and fill it with the data it wants
    if(not_present)
    {
        void *kaddr = get_unused_frame(thread_current(), upage);
        faulted_page->kaddr = kaddr;
        // We want to copy the memory of the physical memory into the frame table.
        // There are three cases, swap, file and 0s
        if(faulted_page->file_name[0] == '\0' || faulted_page->elf_status == faulted_ELF) // All zeros or swap
        {
            if (faulted_page->swap_index == -1) // Not swapped yet, zero it out
                memset(kaddr, 0, PGSIZE);
            else
                swap_in_page(upage);
            // else
                // TODO Need to swap in

        }

        else // It's part of a file, copy in the part we need
        {
            // First, if this is ELF stuff, we need future faults to come from swap
            // This could probably be done better to save io by conditioning on
            // whether writing is allowed to the ELF 
            if (faulted_page->elf_status == nonfaulted_ELF)
                faulted_page->elf_status = faulted_ELF;

            // Will have to think harder about general concurrency, but this is
            // file stuff, so we at least need to lock the filesystem on it
            lock_acquire(&filesys_lock);
            // Since memory mapping has got to work even when the file is closed,
            // I think it has to be done this way
            struct file *f = filesys_open(faulted_page->file_name);
            file_seek(f, faulted_page->file_offset);
            int bytes_written = file_read(f, kaddr, faulted_page->bytes_to_read);
            memset(kaddr + bytes_written, 0, PGSIZE - bytes_written);
            file_close(f);
            lock_release(&filesys_lock);
        }
        // Now the frame should be filled with all the right data. Time to register it
        pagedir_set_page(thread_current()->pagedir, upage, kaddr, ! faulted_page->read_only);
    }
}

