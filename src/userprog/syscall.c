#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"

#define MAX_FILES 256 // Arbitrary limit on number of files
#include "devices/shutdown.h"
#include "devices/input.h"
//#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

static void syscall_handler(struct intr_frame *);

static int sys_filesize (int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_seek(int fd, unsigned pos);
static unsigned sys_tell(int fd);
static void sys_close(int fd);
static int find_available_fd(void);

static void syscall_exit(int status);

static void * ptr_is_valid(const void *ptr);

static bool is_white_space(char input)
{
   return (input == '\0' || input == '\r' || input == '\n');
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
    if (ptr_is_valid((void*) name) != NULL)
    {
        process_execute((char*)ptr_is_valid((void*) name));
    }
    else
    {
        syscall_exit(-1);
    }
}
static bool syscall_create(const char * file, unsigned size)
{
    //maybe we need something else here?
    if (ptr_is_valid((void*) file) != NULL)
    {
        return filesys_create((char*)ptr_is_valid((void*) file), size);
    }
    else
    {
        syscall_exit(-1);
    }
    return 0;
}

static bool syscall_remove(const char *file)
{
    // gotta do some other check maybe???
    char * f = (char *) ptr_is_valid((void*) file);
    if (! is_white_space(*f))
    {
        return filesys_remove(f);
    }
    else
    {
        syscall_exit(-1);
    }
    return 0;
}
static int syscall_open(const char *file)
{
    char * f = (char *) ptr_is_valid((void*) file);
    if (! is_white_space(*f))
    {
        int fd = find_available_fd();
        thread_current()->open_files[fd] = filesys_open(f);
        if (thread_current()->open_files[fd] == NULL)
        {
            return -1;
        }
        // deny writes to executables
        
        return fd;
    }
    else
    {
        syscall_exit(-1);
    }
    return 0;
}
static int syscall_wait(int pid)
{
    return process_wait(pid);
}

static mapid_t syscall_mmap(int fd, void *addr)
{
    lock_acquire(&filesys_lock);
    struct file *this_file = thread_current()->open_files[fd];
    int file_size = file_length(this_file);
    int curr_pos = 0;
    while(curr_pos < file_size)
    {
	//make new suppl_page table entry
        suppl_page * page = new_suppl_page(this_file->deny_write, addr + curr_pos, NULL, this_file->file_name, 
   curr_pos, ((PGSIZE < file_size - curr_pos) ? PGSIZE : file_size - curr_pos));

        hash_insert(&thread_current()->suppl_page_table, &page->hash_elem);
        curr_pos += PGSIZE;
    }	
 
    lock_release(&filesys_lock);
    // TODO figure out what to return
    return 1;
}

static void syscall_handler(struct intr_frame *f UNUSED) {

    long intr_num;
    long *arg0;
    long *arg1;
    long *arg2;

    if (ptr_is_valid((void*) f->esp) != NULL)
    {
        intr_num = *((long *) f->esp) ;
        arg0 = (((long *) f->esp) + 1); 
        arg1 = (((long *) f->esp) + 2); 
        arg2 = (((long *) f->esp) + 3); 
    }
    else
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
            if(ptr_is_valid(arg0))
            {
                syscall_exit(*arg0);
            }
            else
            {
                syscall_exit(-1);
            }
            break;
        case  SYS_EXEC:                   /*!< Start another process. */
            if(ptr_is_valid(arg0))
            {
		syscall_exec(*(char **) arg0);
            }
            else
            {
                syscall_exit(-1);
            }
            break;
        case  SYS_WAIT:                   /*!< Wait for a child process to die. */
            // process wait
            if(ptr_is_valid(arg0))
            {
	        syscall_wait((int) *arg0);
            }
            else
            {
                syscall_exit(-1);
            } 
        case  SYS_CREATE:                 /*!< Create a file. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0) && ptr_is_valid(arg1))
            {
                syscall_create(*(char **) arg0, (int) *arg1);
            }
            else
            {
                syscall_exit(-1);
            }
            lock_release(&filesys_lock);
            break;
        case  SYS_REMOVE:                 /*!< Delete a file. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0))
            {
		syscall_remove(*(char **) arg0);
            }
            else
            {
                syscall_exit(-1);
            }
            lock_release(&filesys_lock);
            break;
        case  SYS_OPEN:                   /*!< Open a file. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0))
            {
                syscall_open(*(char **) arg0);
            }
            else
            {
                syscall_exit(-1);
            }

            lock_release(&filesys_lock);
            break;
        case  SYS_FILESIZE:               /*!< Obtain a file's size. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0))
            {
                f->eax = sys_filesize(*arg0);
            }
            else
            {
                syscall_exit(-1);
            }
            lock_release(&filesys_lock);
            break;
        case  SYS_READ:                   /*!< Read from a file. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0) && ptr_is_valid(arg1) && ptr_is_valid(arg2))
            {
                f->eax = sys_read(*arg0, *(void **)arg1, *arg2);
            }
            else
            {
                syscall_exit(-1);
            }
            lock_release(&filesys_lock);
            break;
        case  SYS_WRITE:                  /*!< Write to a file. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0) && ptr_is_valid(arg1) && ptr_is_valid(arg2))
            {
                f->eax = sys_write(*arg0,* (void **)arg1, *arg2);
            }
            else
            {
                syscall_exit(-1);
            }
            lock_release(&filesys_lock);
            break;
        case  SYS_SEEK:                   /*!< Change position in a file. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0) && ptr_is_valid(arg1))
            {
		sys_seek(*arg0, *arg1);                
            }
            else
            {
                syscall_exit(-1);
            }           
            lock_release(&filesys_lock);
            break;
        case  SYS_TELL:                   /*!< Report current position in a file. */
            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0))
            {
                f->eax = sys_tell(*arg0);
            }
            else
            {
                syscall_exit(-1);
            }
            lock_release(&filesys_lock);
            break;
        case  SYS_CLOSE:                  /*!< Close a file. */

            lock_acquire(&filesys_lock);
            if(ptr_is_valid(arg0))
            {
                sys_close(*arg0);
            }
            else
            {
                syscall_exit(-1);
            }

            lock_release(&filesys_lock);
            break;

        case  SYS_MMAP:                   /*!< Map a file into memory. */
	    syscall_mmap(arg0, arg1);           
            break;
        case  SYS_MUNMAP:                 /*!< Remove a memory mapping. */
            // TODO
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

static int sys_filesize(int fd)
{
    if (check_fd(fd))
        return file_length(thread_current()->open_files[fd]); 

    thread_exit(); // File doesn't exist. Let's destroy this process
}

static int sys_read(int fd, void *buffer, unsigned size)
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

    thread_exit(); // File doesn't exist. Let's destroy this process
}

static int sys_write(int fd, const void *buffer, unsigned size)
{
    if (fd == 1)
    {
        putbuf(buffer, size);
        return size;
    }
    if (check_fd(fd))
        return file_write(thread_current()->open_files[fd], buffer, size);

    thread_exit(); // File doesn't exist. Let's destroy this process
}

static void sys_seek(int fd, unsigned pos)
{
    if (check_fd(fd))
        file_seek(thread_current()->open_files[fd], pos); 
    else
        thread_exit(); // File doesn't exist. Let's destroy this process
}

static unsigned sys_tell(int fd)
{
    if (check_fd(fd))
        return file_tell(thread_current()->open_files[fd]); 

    thread_exit(); // File doesn't exist. Let's destroy this process
}

static void sys_close(int fd)
{
    if (check_fd(fd))
        file_close(thread_current()->open_files[fd]); 
    else
        thread_exit(); // File doesn't exist. Let's destroy this process
}
