#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"
#include "threads/malloc.h"

#define MAX_FILES 256 // Arbitrary limit on number of files
#include "devices/shutdown.h"
#include "devices/input.h"
//#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

static void syscall_handler(struct intr_frame *);

static void syscall_exit(int status);

static void * ptr_is_valid(const void *ptr);

static bool is_white_space(char input)
{
   return (input == '\0' || input == '\r' || input == '\n');
}

static bool is_valid_filename(const char *file)
{
    return file != NULL
        && file[0] != '\0'
        && strlen(file) < 15
        && strlen(file) > 0
        && !is_white_space(file[0]);
}

// check whether a passed in pointer is valid, return correct address
// currently doing it the slower way, will attempt to implemenent 2nd way
// if time remains
static void * ptr_is_valid(const void *ptr)
{   // check whether the ptr address is valid and within user virtual memory   
    // page directory is most significant 10 digits of vaddr
    // mimicking pd_no. PDSHIFT = (PTSHIFT + PTBITS) = PGBITS + 10 = 22
    if (!(is_user_vaddr(ptr)) || ptr == NULL) // if it isn't
    {
        // how do compare ptr < 0?
        // destroy page
        //pagedir_destroy((uint32_t *) pd);
        syscall_exit(-1);
    }
    // if the location is valid:
    // lookup page uses page directory (most significant 10 digits of
    // vaddr), vaddr, and CREATE = true(if not found, 
    // create new page table, return its ptr) or false(if not found,
    // return null)
    return pagedir_get_page(thread_current()->pagedir, ptr);
}

void syscall_init(void) {
    lock_init(&filesys_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_halt(void)
{
    shutdown_power_off();
}

static void syscall_exit(int status)
{
    // find a way to return the status
    thread_current()->exit_val = status;
    // We set up the DYING vs WAITING in thread_exit
    thread_exit();
}
static void syscall_exec(char * name)
{
    if (is_valid_filename(name))
    {
        process_execute(name);
    }
    else
    {
        syscall_exit(-1);
    }
}
static bool syscall_create(const char * file, unsigned size)
{
    //maybe we need something else here?
    if (is_valid_filename(file))
    {
        return filesys_create(file, size);
    }
    // Special cases
    if (file == NULL || file[0] == '\0')
        syscall_exit(-1);
    return false;
}

static bool syscall_remove(const char *file)
{
    // gotta do some other check maybe???
    if (is_valid_filename(file))
    {
        return filesys_remove(file);
    }
    else
    {
        syscall_exit(-1);
    }
    return 0;
}

// This'll be useful for opening a new file
static int find_available_fd(void)
{
    int i;
    for (i = 2; i < MAX_FILES; i++)
    {
        if (thread_current()->open_files[i] != NULL)
            return i;
    }
    return -1;
}

static bool check_fd(int fd)
{
    return fd >= 0 && fd < MAX_FILES && (thread_current()->open_files[fd] != NULL);
}

static int syscall_open(const char *file)
{
    if (is_valid_filename(file))
    {
        int fd = find_available_fd();
        thread_current()->open_files[fd] = filesys_open(file);
        if (thread_current()->open_files[fd] == NULL)
        {
            return -1;
        }
        // deny writes to executables

        return fd;
    }
    // Seemingly arbitrary different ways of dealing with different bad input
    if (file != NULL && file[0] == '\0')
        return -1;
    syscall_exit(-1);
    return -1;
}


static int syscall_filesize(int fd)
{
    if (check_fd(fd))
        return file_length(thread_current()->open_files[fd]); 
    syscall_exit(-1); // File doesn't exist. Let's destroy this process
    return -1; // Keep the compiler happy
}

static int syscall_read(int fd, void *buffer, unsigned size)
{
    if (fd == 0)
    {
        unsigned i;
        for (i = 0; i < size; i++)
            ((char *)buffer)[i] = input_getc();
        return size;
    }
    if (check_fd(fd))
        return file_read(thread_current()->open_files[fd], buffer, size);

    syscall_exit(-1); // File doesn't exist. Let's destroy this process
    return -1; // Keep the compiler happy 
}

static int syscall_write(int fd, const void *buffer, unsigned size)
{
    if (fd == 1)
    {
        putbuf(buffer, size);
        return size;
    }
    if (check_fd(fd))
        return file_write(thread_current()->open_files[fd], buffer, size);

    syscall_exit(-1); // File doesn't exist. Let's destroy this process
    return -1; // Keep the compiler happy
}

static void syscall_seek(int fd, unsigned pos)
{
    if (check_fd(fd))
        file_seek(thread_current()->open_files[fd], pos); 
    else
        syscall_exit(-1); // File doesn't exist. Let's destroy this process
}

static unsigned syscall_tell(int fd)
{
    if (check_fd(fd))
        return file_tell(thread_current()->open_files[fd]); 

    syscall_exit(-1); // File doesn't exist. Let's destroy this process
    return -1; // Keep the compiler happy
}

static void syscall_close(int fd)
{
    if (check_fd(fd))
        file_close(thread_current()->open_files[fd]); 
    else
        syscall_exit(-1); // File doesn't exist. Let's destroy this process
}



static int syscall_wait(int pid)
{
    return process_wait(pid);
}

struct map
{
    mapid_t id;
    struct list_elem elem;
    struct list pages;
};

static mapid_t syscall_mmap(int fd, void *addr)
{
    static mapid_t map_id = 0;
    ASSERT(pg_ofs(addr) == 0);
    struct file *this_file = thread_current()->open_files[fd];
    int file_size = file_length(this_file);
    int curr_pos = 0;
    struct map curr_map;
    while(curr_pos < file_size)
    {
        //make new suppl_page table entry
        unsigned bytes_to_read = (file_size - curr_pos < PGSIZE) ? file_size - curr_pos
                                                                 : PGSIZE;
        lock_acquire(&thread_current()->suppl_lock);
        suppl_page * page = new_suppl_page(this_file->deny_write, addr + curr_pos); 
        set_suppl_page_file(page, this_file->file_name, curr_pos, bytes_to_read);

        list_push_back (&curr_map.pages, &page->elem);
        hash_insert(&thread_current()->suppl_page_table, &page->hash_elem);
        lock_release(&thread_current()->suppl_lock);
        curr_pos += PGSIZE;
    }

    // Each map has a unique id.
    // Technically, this is a problem if we have > MAXINT maps preformed, but 
    // that is unlikely to cause a problem.
    map_id ++;
    curr_map.id = map_id;
    list_push_back(&thread_current()->maps, &curr_map.elem);
    return map_id;
}

static void syscall_munmap(mapid_t id)
{
    struct list_elem * e = list_head (&thread_current()->maps);
    while ((e = list_next (e)) != list_end (&thread_current()->maps))
    {
        if(list_entry(e, struct map, elem)->id == id)
            break;
    }
    // Now we have that e which has the correct map_id
    // So we can clean up all of the attached pages
    // We want to remove from the suppl_page table
    // If it is paged in, we need to write it out to the file

    // We need a lock on the suppl_page table to be sure that the elements
    // aren't changed while we remove some of them.
    lock_acquire(&thread_current()->suppl_lock);
    struct map * curr_map = list_entry(e, struct map, elem);
    struct list_elem * e2 = list_head(&curr_map->pages);
    while ((e2 = list_next (e2)) != list_end (&curr_map->pages))
    {
        struct suppl_page *curr_page = (list_entry(e2, struct suppl_page, elem));
        uint32_t * pos_in_frame = lookup_page(thread_current()->pagedir, curr_page->vaddr, false);
        if(pos_in_frame == NULL) // Then it isn't in the frame table
        {
        }
        else // Then we have the address of the pagetable entry
        {
        // TODO replace this with evicting the page using evict_page from vm/frame.c once it is updated
            // Write out to the file
            lock_acquire(&filesys_lock);
            struct file * curr_file = filesys_open(curr_page->file_name);
            // In theory, this could fail, but since we know that such a file
            // is in the frame, we shouldn't have to worry about failures.
            file_write(curr_file, curr_page->kaddr, curr_page->bytes_to_read);
            lock_release(&filesys_lock);
        }
        free(curr_page);
    }
    lock_release(&thread_current()->suppl_lock);
}
// TODO figure out list_init

static void syscall_handler(struct intr_frame *f) {

    long intr_num;
    long *arg0;
    long *arg1;
    long *arg2;

    if (ptr_is_valid((void*) f->esp) != NULL)
    {
        intr_num = *((long *) f->esp);
        arg0 = (((long *) f->esp) + 1); 
        arg1 = (((long *) f->esp) + 2); 
        arg2 = (((long *) f->esp) + 3); 
    }
    else
    {
        syscall_exit(-1);
    }
    // The tests imply this is necessary even when none of the arguments are needed
    if (!is_user_vaddr(arg0))
    {
        syscall_exit(-1);
    }

    switch (intr_num)
    {
        /* Projects 2 and later. */
        case  SYS_HALT:                   /*!< Halt the operating system. */
            printf("syscall: ");
            syscall_halt();
            break;
        case  SYS_EXIT:                   /*!< Terminate this process. */
            syscall_exit(*arg0);
            break;
        case  SYS_EXEC:                   /*!< Start another process. */
            syscall_exec((char *) ptr_is_valid((char *) *arg0));
            break;
        case  SYS_WAIT:                   /*!< Wait for a child process to die. */
            // process wait
            syscall_wait((int) *arg0);
        case  SYS_CREATE:                 /*!< Create a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_create((char *) (ptr_is_valid((char *) *arg0)), *arg1);
            lock_release(&filesys_lock);
            break;
        case  SYS_REMOVE:                 /*!< Delete a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_remove((char *) ptr_is_valid((char *) *arg0));
            lock_release(&filesys_lock);
            break;
        case  SYS_OPEN:                   /*!< Open a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_open((char *) ptr_is_valid((char *) *arg0));
            lock_release(&filesys_lock);
            break;
        case  SYS_FILESIZE:               /*!< Obtain a file's size. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_filesize(*arg0);
            lock_release(&filesys_lock);
            break;
        case  SYS_READ:                   /*!< Read from a file. */
            // while we have access to the vaddr, check it it's read_only
            ptr_is_valid((void *) *arg1); // First make sure the address is real
            struct thread *curr = thread_current();
            lock_acquire(&thread_current()->suppl_lock);
            suppl_page *pg = suppl_page_lookup(&curr->suppl_page_table, (void *) *arg1);
            if (pg != NULL && pg->read_only) // NULL is possible because stack extensions are legal
                syscall_exit(-1);
            lock_release(&thread_current()->suppl_lock);
            lock_acquire(&filesys_lock);
            f->eax = syscall_read(*arg0, ptr_is_valid((void *) *arg1), *arg2);
            lock_release(&filesys_lock);
            break;
        case  SYS_WRITE:                  /*!< Write to a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_write(*arg0, ptr_is_valid((void *) *arg1), *arg2);
            lock_release(&filesys_lock);
            break;
        case  SYS_SEEK:                   /*!< Change position in a file. */
            lock_acquire(&filesys_lock);
            syscall_seek(*arg0, *arg1);
            lock_release(&filesys_lock);
            break;
        case  SYS_TELL:                   /*!< Report current position in a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_tell(*arg0);
            lock_release(&filesys_lock);
            break;
        case  SYS_CLOSE:                  /*!< Close a file. */
            lock_acquire(&filesys_lock);
            syscall_close(*arg0);
            lock_release(&filesys_lock);
            break;
        case  SYS_MMAP:                   /*!< Map a file into memory. */
            f->eax = syscall_mmap(*arg0, ptr_is_valid((void *) *arg1));
            break;
        case  SYS_MUNMAP:                 /*!< Remove a memory mapping. */
            syscall_munmap(*arg0);
            break;

        // case  SYS_CHDIR:                  /*!< Change the current directory. */
        //     break;
        // case  SYS_MKDIR:                  /* !< Create a directory. */
        //     break;
        // case  SYS_READDIR:                /*!< Reads a directory entry. */
        //     break;
        // case  SYS_ISDIR:                  /*!< Tests if a fd represents a directory. */
        //     break;
        // case  SYS_INUMBER:                 /*!< Returns the inode number for a fd. */
        //     break;
        default:
            syscall_exit(-1);
    }
}
