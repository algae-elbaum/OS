#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

/* Root layer inode. Points to level one or two or disk */
struct inode_disk_root {
    off_t length;                       /*!< File size in bytes. */
    unsigned magic;                     /*!< Magic number. */
    block_sector_t sectors[100];
    struct inode_disk_two *twos[25];
    struct inode_disk_one * one;
};
/* First layer inode. It comes from root and points to level two */
struct inode_disk_one {
    // Pointers shouldn't be larger than 1 byte, so we can point to
    // 512 level two inodes. Since each of those is 64kB, a level one
    // supports 32 MB.
    struct inode_disk_two* twos[BLOCK_SECTOR_SIZE / sizeof(struct inode_disk_two *)];
};
/* Second layer inode. Comes from level one or root and points to disk */
struct inode_disk_two {
    // We want to store an array of sector numbers and max out how many we have
    // This is 128 sectors. Which is 64 kB.
    block_sector_t sectors[BLOCK_SECTOR_SIZE / sizeof(block_sector_t)];
};

void inode_init(void);
bool inode_create(block_sector_t, off_t);
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);

block_sector_t byte_to_sector(struct inode *inode, off_t  byte);
bool writes_forbidden(const struct inode *inode);

block_sector_t * num_to_sec(struct inode_disk_root *root, int sec_num);

#endif /* filesys/inode.h */
