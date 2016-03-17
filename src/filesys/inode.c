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
block_sector_t byte_to_sector(struct inode *inode, off_t byte) {
    ASSERT(inode != NULL);
    int sec = byte / BLOCK_SECTOR_SIZE;
    return *num_to_sec(&inode->data, sec, false);
}

block_sector_t * num_to_sec(struct inode_disk_root *root, int sec_num, bool write_flag)
{
    int temp1;
    if(sec_num < 100)
        return &root->sectors[sec_num];
    if(sec_num < 100 + 25*128)
    {
        sec_num -= 100;
        temp1 = sec_num / 128;
        if (write_flag && &root->twos[temp1]->sectors[sec_num - 128*temp1] == NULL)
            root->twos[temp1] = malloc(sizeof(struct inode_disk_two));
        return &root->twos[temp1]->sectors[sec_num - 128*temp1];
    }
    else
    {
        sec_num -= (100 + 25*128);
        temp1 = sec_num / 512;
        if(write_flag && &root->one == NULL)
            root->one = malloc(sizeof(struct inode_disk_one));
        if (write_flag && &root->one->twos[temp1]->sectors[sec_num - 512*temp1] == NULL)
            root->one->twos[temp1] = malloc(sizeof(struct inode_disk_two));
        return &root->one->twos[temp1]->sectors[sec_num - 512*temp1];
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
    ASSERT(sizeof(struct inode_disk_root) == BLOCK_SECTOR_SIZE);

    root = calloc(1, sizeof(struct disk_inode_root*));
    if (root != NULL) {
        size_t sectors = bytes_to_sectors(length);
        root->length = length;
        root->magic = INODE_MAGIC;
        bool all_good = true;
        size_t i;
        block_sector_t sec;
        for(i = 0; i < sectors; i++)
        {
            if(! free_map_allocate(1, &sec))
            {
                all_good = false;
                break;
            }
            *num_to_sec(root, i, true) = sec;
        }
        if(! all_good)
        {
            i++;
            for(; i > 0; i --)
            {
                free_map_release(*num_to_sec(root,i-1,false), 1);
            }
        }
        if (all_good) {
            block_write(fs_device, sector, root);
            if (sectors > 0) {
                static char zeros[BLOCK_SECTOR_SIZE];
                for (i = 0; i < sectors; i++)
                {
                    block_write(fs_device, *num_to_sec(root, i,true), zeros);
                }
            }
            success = true; 
        }
        free(root);
    }
    return success;
}

/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    // rw_acquire_read(&open_inodes_rw_lock);
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            // rw_release_read(&open_inodes_rw_lock);
            return inode; 
        }
    }
    // rw_release_read(&open_inodes_rw_lock);

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    // rw_acquire_write(&open_inodes_rw_lock);
    list_push_front(&open_inodes, &inode->elem);
    // rw_release_write(&open_inodes_rw_lock);
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
    // rw_acquire_write(&open_inodes_rw_lock);
    if (inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);
        // rw_release_write(&open_inodes_rw_lock);

        /* Deallocate blocks if removed. */
        if (inode->removed) {
            free_map_release(inode->sector, 1);
//            free_map_release(inode->data.start,
  //                           bytes_to_sectors(inode->data.length)); 
            int i;
            for (i = 0; i < inode->data.length; i ++)
            {
                free_map_release(*num_to_sec(&inode->data, i,false), 1);
            }
            free(inode); 
        }
    }
    else
    {
        // rw_release_write(&open_inodes_rw_lock);
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

