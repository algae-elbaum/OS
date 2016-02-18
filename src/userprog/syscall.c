#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include"userprog/process.h"

#define MAX_FILES 256 // Arbitrary limit on number of files
#include "devices/shutdown.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}
static void set_return(struct intr_frame *f UNUSED, uint32_t input)
{
    (f->eax) = input;
}

static void syscall_halt(void)
{
    shutdown_power_off();
}

static void syscall_exit(int status)
{
    // find a way to return the status?????

    thread_exit();
}
static void syscall_exec(char * name)
{
    process_execute(name);
}
static void syscall_create(char * file, int size)
{
    //TODO algae
}

////// File syscalls \\\\\\\/

// I'd like to identify file descriptors with processes somehow, but it doesn't seem like the 
// intr_frame holds anything uniquely identifying. Eventually should use thread_current for some
// sort of map, but for now I just want to get this working
// So instead a user process could perfectly well use a file descriptor that wasn't given to it
static struct file *open_files[MAX_FILES];
static int used_fds[MAX_FILES] = {0};

// Gets the first free file descriptor. If none are available, returns -1
static int find_available_fd(void)
{
    int i;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (! used_fds[i])
            return i;
    }
}


static int sys_write(int fd, const void *buffer, unsigned size)
{
//    printf("%d", fd);
    return 1;
    if (fd == 1)
    {
        putbuf(buffer, size);
        return size;
    }
    if (used_fds[fd])
        return file_write (open_files[fd], buffer, size);
    return -1; // Or maybe call sys_exit for murder fun time
}

static void syscall_handler(struct intr_frame *f UNUSED) {
    long intr_num = *((long *) f->esp) ;
    long arg0 = *(((long *) f->esp) + 1); 
    long arg1 = *(((long *) f->esp) + 2); 
    long arg2 = *(((long *) f->esp) + 3); 
    switch (intr_num)
    {
        /* Projects 2 and later. */
        case  SYS_HALT:                   /*!< Halt the operating system. */
            printf("syscall: ");
            syscall_halt();
            break;
        case  SYS_EXIT:                   /*!< Terminate this process. */
            syscall_exit(arg0);
            break;
        case  SYS_EXEC:                   /*!< Start another process. */
            syscall_exec((char *) arg0);
            break;
        case  SYS_WAIT:                   /*!< Wait for a child process to die. */
            // prpocess wait
        case  SYS_CREATE:                 /*!< Create a file. */
            syscall_create((char *) arg0, (int) arg1);
            break;
        case  SYS_REMOVE:                 /*!< Delete a file. */
            break;
        case  SYS_OPEN:                   /*!< Open a file. */
            break;
        case  SYS_FILESIZE:               /*!< Obtain a file's size. */
            break;
        case  SYS_READ:                   /*!< Read from a file. */
            break;
        case  SYS_WRITE:                  /*!< Write to a file. */
            f->eax = sys_write(arg0, (void *)arg1, arg2);
            break;
        case  SYS_SEEK:                   /*!< Change position in a file. */
            break;
        case  SYS_TELL:                   /*!< Report current position in a file. */
            break;
        case  SYS_CLOSE:                  /*!< Close a file. */
            break;

        // These are for later projects. 

        /* Project 3 and optionally project 4. */
        // case  SYS_MMAP:                   /*!< Map a file into memory. */
        //     break;
        // case  SYS_MUNMAP:                 /*!< Remove a memory mapping. */
        //     break;

        // /* Project 4 only. */
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
            printf("system call!\n");
            thread_exit();
    }
}


