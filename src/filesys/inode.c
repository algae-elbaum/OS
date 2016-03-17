#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define A_INC(X) {__sync_add_and_fetch (&(X), 1);};  // Amazing (atomically increment)
#define A_DEC(X) {__sync_add_and_fetch (&(X), -1);}; // Amazing (atomically decrement)

/* Root layer inode. Points to level one or two or disk */
struct inode_disk_root {
    off_t length;                       /*!< File size in bytes. */
    unsigned magic;                     /*!< Magic number. */
    block_sector_t *sectors[100];
    inode_disk_two *twos[25];
    inode_disk_one * one;
};
/* First layer inode. It comes from root and points to level two */
struct inode_disk_one {
    // Pointers shouldn't be larger than 1 byte, so we can point to
    // 512 level two inodes. Since each of those is 64kB, a level one
    // supports 32 MB.
    inode_disk_two *twos[BLOCK_SECTOR_SIZE / sizeof(inode_disk_two *)];
};
/* Second layer inode. Comes from level one or root and points to disk */
struct inode_disk_two {
    // We want to store an array of sector numbers and max out how many we have
    // This is 128 sectors. Which is 64 kB.
    block_sector_t sectors[BLOCK_SECTOR_SIZE / sizeof(block_sector_t)];
};

/*! Returns the number of sectors to allocate for an inode SIZE
    bytes long. */
static inline size_t bytes_to_sectors(off_t size) {
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/*! In-memory inode. */
struct inode {
    struct list_elem elem;              /*!< Element in inode list. */
    block_sector_t sector;              /*!< Sector number of disk location. */
    int open_cnt;                       /*!< Number of openers. */
    bool removed;                       /*!< True if deleted, false otherwise. */
    int deny_write_cnt;                 /*!< 0: writes ok, >0: deny writes. */
    struct inode_disk_root data;             /*!< Inode content. */
};

/*! Returns the block device sector that contains byte offset POS
    within INODE.
    */
block_sector_t byte_to_sector(const struct inode *inode, off_t sec) {
    ASSERT(inode != NULL);
    int temp1;
    if(sec < 100)
        return root->sectors[sec];
    else if(sec >= 100 && sec < 100 + 25*128){
        temp1 = (sec - 100)/128;
        return root->twos[temp1]->sectors[(sec - 100) - temp1 * 128];
    }
    else {
    // Well we need to use the doubly indirect inodes
        sec -= 100+25*128;
        temp1 = sec / 128;
        return root->one->twos[temp1];
    }
}

bool writes_forbidden(const struct inode *inode)
{
    return inode->deny_write_cnt;
}

/*! List of open inodes, so that opening a single inode twice
    returns the same `struct inode'. */
static struct list open_inodes;
static struct rw_lock open_inodes_rw_lock;

/*! Initializes the inode module. */
void inode_init(void) {
    list_init(&open_inodes);
    rw_init(&open_inodes_rw_lock);
}

/*! Initializes an inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length) {
    struct inode_disk_root *root = NULL;
    bool success = false;

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *inode_disk_root == BLOCK_SECTOR_SIZE);

    root = calloc(1, sizeof *disk_inode_root);
    if (root != NULL) {
        size_t sectors = bytes_to_sectors(length);
        root->length = length;
        root->magic = INODE_MAGIC;
        if (free_map_allocate(sectors, &root->sectors[0]) {
            block_write(fs_device, sector, root);
            if (sectors > 0) {
                static char zeros[BLOCK_SECTOR_SIZE];
                size_t i;
                for (i = 0; i < sectors; i++)
                    block_write(fs_device, byte_to_sector(root, i), zeros);
            }
            success = true; 
        }
        free(root);
    }
    return success;
}

static void write_sector(disk_inode_root * root, int sec, void* buffer)
{
    int temp1;
    if(sec < 100)
        block_write(fs_device, root->sectors[sec], buffer);
    else if(sec >= 100 && sec < 100 + 25*128){
        temp1 = (sec - 100)/128;
        block_write(fs_device, root->twos[temp1]->sectors[(sec - 100) - temp1 * 128], buffer);
    }
    else {
    // Well we need to use the doubly indirect inodes
        sec -= 100+25*128;
        temp1 = sec / 128;
        block_write(fs_device, root->one->twos[temp1], buffer);
    }
}

/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    rw_acquire_read(&open_inodes_rw_lock);
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            rw_release_read(&open_inodes_rw_lock);
            return inode; 
        }
    }
    rw_release_read(&open_inodes_rw_lock);

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    rw_acquire_write(&open_inodes_rw_lock);
    list_push_front(&open_inodes, &inode->elem);
    rw_release_write(&open_inodes_rw_lock);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    block_read(fs_device, inode->sector, &inode->data);
    return inode;
}

/*! Reopens and returns INODE. */
struct inode * inode_reopen(struct inode *inode) {
    if (inode != NULL)
        A_INC(inode->open_cnt);
    return inode;
}

/*! Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode *inode) {
    return inode->sector;
}

/*! Closes INODE and writes it to disk.
    If this was the last reference to INODE, frees its memory.
    If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
    /* Ignore null pointer. */
    if (inode == NULL)
        return;

    /* Release resources if this was the last opener. */
    A_DEC(inode->open_cnt);
    if (inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        rw_acquire_write(&open_inodes_rw_lock);
        list_remove(&inode->elem);
        rw_release_write(&open_inodes_rw_lock);

        // Make sure nothing incremented the open count while it was blocked
        if (inode->open_cnt == 0)
        {
            /* Deallocate blocks if removed. */
            if (inode->removed) 
            {
                free_map_release(inode->sector, 1);
                free_map_release(inode->data.start,
                                 bytes_to_sectors(inode->data.length)); 
            }

            free(inode); 
        }
    }
}

/*! Marks INODE to be deleted when it is closed by the last caller who
    has it open. */
void inode_remove(struct inode *inode) {
    ASSERT(inode != NULL);
    inode->removed = true;
}

/*! Disables writes to INODE.
    May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) {
    A_INC(inode->deny_write_cnt);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/*! Re-enables writes to INODE.
    Must be called once by each inode opener who has called
    inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode) {
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/*! Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode) {
    return inode->data.length;
}

