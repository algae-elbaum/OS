#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED) {
#if 0
    switch (*((int *) f->esp) of)
    {
        /* Projects 2 and later. */
        case  SYS_HALT:                   /*!< Halt the operating system. */
            printf("syscall: ");
            break;
        case  SYS_EXIT:                   /*!< Terminate this process. */
        case  SYS_EXEC:                   /*!< Start another process. */
        case  SYS_WAIT:                   /*!< Wait for a child process to die. */
        case  SYS_CREATE:                 /*!< Create a file. */
        case  SYS_REMOVE:                 /*!< Delete a file. */
        case  SYS_OPEN:                   /*!< Open a file. */
        case  SYS_FILESIZE:               /*!< Obtain a file's size. */
        case  SYS_READ:                   /*!< Read from a file. */
        case  SYS_WRITE:                  /*!< Write to a file. */
        case  SYS_SEEK:                   /*!< Change position in a file. */
        case  SYS_TELL:                   /*!< Report current position in a file. */
        case  SYS_CLOSE:                  /*!< Close a file. */

        /* Project 3 and optionally project 4. */
        case  SYS_MMAP:                   /*!< Map a file into memory. */
        case  SYS_MUNMAP:                 /*!< Remove a memory mapping. */

        /* Project 4 only. */
        case  SYS_CHDIR:                  /*!< Change the current directory. */
        case  SYS_MKDIR:                  /*!< Create a directory. */
        case  SYS_READDIR:                /*!< Reads a directory entry. */
        case  SYS_ISDIR:                  /*!< Tests if a fd represents a directory. */
        case  SYS_INUMBER:                 /*!< Returns the inode number for a fd. */
     
    }
#endif
    printf("system call!\n");
    thread_exit();
}

