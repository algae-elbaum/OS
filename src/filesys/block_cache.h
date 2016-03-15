#include "inode.h"

void cache_init(void);
off_t cache_read(struct inode *inode, void *buffer_, off_t size, off_t offset);
bool cache_write(struct inode *inode, const void *buffer_, off_t size, off_t offset);