#include "inode.h"

void cache_init(void);
void cache_periodic_read_ahead(void);
void cache_periodic_flush(void);
off_t cache_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset);
off_t cache_write_at(struct inode *inode, const void *buffer_, off_t size, off_t offset);