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
#include "devices/shutdown.h"
#include "devices/input.h"
//#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

struct lock filesys_lock;

static void syscall_handler(struct intr_frame *);

static void syscall_exit(int status);

static void * check_and_convert_ptr(const void *ptr);

static bool is_white_space(char input)
{
   return (input == ' ' || input == '\0' || input == '\r' || input == '\n');
}

static bool is_valid_filename(const char *file)
{
    return file != NULL
        && !is_white_space(file[0])
        && strlen(file) < 15
        && strlen(file) > 0;
}

static bool is_valid_command(const char *command)
{

    if (command != NULL)
    {
        int file_len = 0;
        while (!is_white_space(command[file_len]))
            file_len ++;

        return file_len < 15 && file_len > 0;
    }
    return false;
}


// check whether a passed in pointer is valid, return correct address
// currently doing it the slower way, will attempt to implemenent 2nd way
// if time remains
static void * check_and_convert_ptr(const void *ptr)
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
static tid_t syscall_exec(char * command)
{
    if (is_valid_command(command))
    {
        tid_t tid = process_execute(command);
        // Wait and see if the exec worked
        struct thread *curr = thread_current();
        lock_acquire(&curr->exec_lock);
        cond_wait(&curr->exec_cond, &curr->exec_lock);
        lock_release(&curr->exec_lock);
        if (curr->exec_success)
            return tid;
        else
            return -1;
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
    struct thread *t = thread_current();
    for (i = 2; i < MAX_FILES; i++)
    {
        if (t->open_files[i] == NULL)
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
    int fd;
    if (is_valid_filename(file) && (fd = find_available_fd()) != -1)
    {
        struct thread *t = thread_current();
        t->open_files[fd] = filesys_open(file);
        if (t->open_files[fd] == NULL)
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
    if (check_fd(fd) && buffer != NULL)
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
    if (check_fd(fd) && buffer != NULL)
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
    {
        struct thread *t = thread_current();
        file_close(t->open_files[fd]); 
        t->open_files[fd] = NULL;
    }
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

static void syscall_handler(struct intr_frame *f) {

    long intr_num;
    long *arg0;
    long *arg1;
    long *arg2;

    if (check_and_convert_ptr((void*) f->esp) != NULL)
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
            f->eax = syscall_exec((char *) check_and_convert_ptr((char *) *arg0));
            break;
        case  SYS_WAIT:                   /*!< Wait for a child process to die. */
            // process wait
            f->eax = syscall_wait((int) *arg0);
            break;
        case  SYS_CREATE:                 /*!< Create a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_create((char *) check_and_convert_ptr((char *) *arg0), *arg1);
            lock_release(&filesys_lock);
            break;
        case  SYS_REMOVE:                 /*!< Delete a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_remove((char *) check_and_convert_ptr((char *) *arg0));
            lock_release(&filesys_lock);
            break;
        case  SYS_OPEN:                   /*!< Open a file. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_open((char *) check_and_convert_ptr((char *) *arg0));
            lock_release(&filesys_lock);
            break;
        case  SYS_FILESIZE:               /*!< Obtain a file's size. */
            lock_acquire(&filesys_lock);
            f->eax = syscall_filesize(*arg0);
            lock_release(&filesys_lock);
            break;
        case  SYS_READ:                   /*!< Read from a file. */
            f->eax = syscall_read(*arg0, check_and_convert_ptr((void *) *arg1), *arg2);
            break;
        case  SYS_WRITE:                  /*!< Write to a file. */
            f->eax = syscall_write(*arg0, check_and_convert_ptr((void *) *arg1), *arg2);
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
        // case  SYS_MMAP:                   /*!< Map a file into memory. */
        //     f->eax = syscall_mmap(*arg0, check_and_convert_ptr((void *) *arg1));
        //     break;
        // case  SYS_MUNMAP:                 /*!< Remove a memory mapping. */
        //     syscall_munmap(*arg0);
        //     break;

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
