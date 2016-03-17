#include "block_cache.h"
#include <debug.h>
#include <string.h>
#include <list.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "devices/timer.h"
#include "devices/block.h"
#include "inode.h"
#include "filesys.h"

#define NUM_SLOTS 64
// I have no idea what good values are for these next two #defines...
#define READ_AHEAD_FREQ 500 // In milliseconds
#define FLUSH_FREQ 500 // Also in milliseconds
#define A_INC(X) {__sync_add_and_fetch (&(X), 1);};  // Amazing
#define A_DEC(X) {__sync_add_and_fetch (&(X), -1);}; // Amazing

typedef struct cache_slot_v // v for volatile
{
    volatile block_sector_t sector; // Sector held in the slot         
    volatile int in_use;            // Count processes using this entry
    volatile int64_t last_used;     // Holds the tick when this was used last
    volatile int trying_to_evict;   /* So that nothing tries to use the block too
                                       late into eviction to stop the eviction */
    volatile bool touched;          // Whether the slot has ever been used
    volatile bool dirty;
} cache_slot_v;

// Separate struct because volatility is apparently infectious
// For the first time in a long time I'm seriously annoyed with C
typedef struct cache_slot_nv // nv for not volatile
{
    uint8_t data[BLOCK_SECTOR_SIZE];  // Actual data (seems like it should be volatile, but memcpy...)
} cache_slot_nv;


typedef struct caching_in_elem
{
    block_sector_t sector;
    int slot;
    struct list_elem elem;
    struct lock lock;
    struct condition cond;
} caching_in_elem;

static struct list caching_in_list;
static struct lock caching_in_list_lock;

static volatile cache_slot_v cache_v[NUM_SLOTS] = {{0}};;
static cache_slot_nv cache_nv[NUM_SLOTS];

typedef struct read_ahead_elem
{
    block_sector_t sector;
    struct inode *inode;
    struct list_elem elem;
} read_ahead_elem;

static struct list read_ahead_list;
static struct lock read_ahead_lock;

static tid_t read_ahead_tid;
static tid_t flush_tid;

static volatile block_sector_t free_slots = NUM_SLOTS;

static int get_cache_slot(block_sector_t, struct inode *, bool);
static void write_out_slot(int);
static void cache_flush(void);
static void cache_periodic_read_ahead(void *);
static void cache_periodic_flush(void *);

void cache_init(void)
{
    list_init(&caching_in_list);
    lock_init(&caching_in_list_lock);
    list_init(&read_ahead_list);
    lock_init(&read_ahead_lock);
    read_ahead_tid = thread_create("read-ahead", PRI_DEFAULT, cache_periodic_read_ahead, NULL);
    flush_tid = thread_create("flushing", PRI_DEFAULT, cache_periodic_flush, NULL);
}

void cache_done(void)
{
    // Why are interrupts even off?
    // This feels really sketchy
    intr_enable();
    cache_flush();
    intr_disable();
}

// Run through the read ahead list pulling each sector into the cache
static void cache_read_ahead(void)
{
    lock_acquire(&read_ahead_lock);
    while (!list_empty(&read_ahead_list))
    {
        struct list_elem *e = list_pop_front(&read_ahead_list);
        lock_release(&read_ahead_lock);
        read_ahead_elem *f = list_entry(e, struct read_ahead_elem, elem);
        get_cache_slot(f->sector, f->inode, false);
        free(f);
        lock_acquire(&read_ahead_lock);
    }
    lock_release(&read_ahead_lock);
}

static void cache_periodic_read_ahead(void *aux UNUSED)
{
    while (true)
    {
        timer_msleep(READ_AHEAD_FREQ);
        cache_read_ahead();
    }
}

// Write every cached sector to disk
static void cache_flush(void)
{
    int i;
    for (i = 0; i < NUM_SLOTS; i++)
    {
        A_INC(cache_v[i].in_use);
        if (! cache_v[i].trying_to_evict)
        {
            write_out_slot(i);
        }
        A_DEC(cache_v[i].in_use);
    }
}

static void cache_periodic_flush(void *aux UNUSED)
{
    while (true)
    {
        timer_msleep(FLUSH_FREQ);
        cache_flush();
    }
}

static void write_out_slot(int slot)
{
    // Write out the entire sector. Files don't overlap sectors, so even if it
    // would matter, nothing past EOF will even be changed from when the slot 
    // was read in
    if (cache_v[slot].dirty)
    {
        block_write(fs_device, cache_v[slot].sector, cache_nv[slot].data);
        cache_v[slot].dirty = false;
    }
    // No need to clear out slot fields, if they need changing read_into_slot
    // will take care of it, and the periodic flushing of cache data shouldn't
    // affect anything but the disk
}

// Read the block into the slot and fill in the struct's fields
static void read_into_slot(int slot, block_sector_t sector)
{
    // Copy in the whole sector, even if past EOF. 
    // The logic for not reading those needs to happen elsewhere anyways.
    block_read (fs_device, sector, cache_nv[slot].data);
    // After reading in, the slot is given right to whatever uses it, so set in_use now
    cache_v[slot].in_use = 1;
    // If the slot was in use before, write_out_slot didn't clear trying_to_evict.
    // (don't want another eviction grabbing the slot and evicting again)
    cache_v[slot].trying_to_evict = 0;
    // Once this is set get_cache_data can find it, so must set at the end.
    cache_v[slot].sector = sector;
}

// Simultaneous calls may chase each other around for a bit due to the
// trying_to_evict check until they get out of synch and one inc of
// trying_to_evict happens early enough that the other call has yet
// to hit the first if in the for.
// It's a little bad, but doesn't seem like it should be too bad.
// TODO deal with it using a trying_to_evict lock
static int find_slot_to_evict(void)
{
    int64_t best_time = timer_ticks();
    int best = -1;
    // 'while' to make sure that at the end nothing grabbed the evictee for use
    while (best == -1 || cache_v[best].in_use != 0 || cache_v[best].trying_to_evict != 1)
    {
        best_time = timer_ticks();
        best = -1;
        int i;
        for (i = 0; i < NUM_SLOTS; i++)
        {
            if (cache_v[i].in_use == 0 && cache_v[i].last_used < best_time 
                && cache_v[i].trying_to_evict == 0)
            {
                if (best != -1)
                    A_DEC(cache_v[best].trying_to_evict); // Amazing
                best = i;
                A_INC(cache_v[best].trying_to_evict); // Amazing
                best_time = cache_v[best].last_used;
            }
        }
    }
    return best;
} 

// Returns an empty slot with the write lock held.
// If there's already an empty slot, nothing will be actually evicted
static int evict_slot(void)
{
    int i;
    // First see if anything really needs to be evicted
    if (free_slots > 0)
    {
        for (i = 0; i < NUM_SLOTS; i++) 
        {
            A_INC(cache_v[i].trying_to_evict); // Amazing            
            // Amazing
            bool was_untouched = __sync_bool_compare_and_swap(&cache_v[i].touched, false, true);
            if (was_untouched)
            {
                A_DEC(free_slots);
                return i;
            }
            A_DEC(cache_v[i].trying_to_evict); // Amazing
        }
    }
    int free_slot = find_slot_to_evict();
    write_out_slot(free_slot);
    return free_slot;
}

static int slot_of(block_sector_t sector)
{
    int i;
    for (i = 0; i < NUM_SLOTS; i++)
    {
        // Prevent the slot from being evicted
        A_INC(cache_v[i].in_use);
        // Make sure the slot wasn't already being evicted
        if (!cache_v[i].trying_to_evict && cache_v[i].sector == sector)
        {
            cache_v[i].last_used = timer_ticks();
            return i;
        }
        // Not the slot we want, current thread should no longer block eviction
        A_DEC(cache_v[i].in_use);
    }
    return -1;
}

// TODO clean this massive thing up
static int get_cache_slot(block_sector_t sector, struct inode *inode, bool not_read_ahead)
{
    // If the sector is already cached in, just return it  
    int new_slot_try1 = slot_of(sector);
    if (new_slot_try1 != -1)
    {
        return new_slot_try1;
    }

    // Otherwise we have to be careful not to bring in the same sector in twice
    // if two different processes ask for it

    // See if anyone is bringing in the sector yet
    lock_acquire(&caching_in_list_lock);
    struct list_elem *e;
    for (e = list_begin (&caching_in_list); e != list_end (&caching_in_list);
         e = list_next (e))
    {
        struct caching_in_elem *f = list_entry (e, struct caching_in_elem, elem);
        // Someone is already bringing the sector in
        if (f->sector == sector)
        {
            // Wait for them to finish
            lock_acquire(&f->lock);
            cond_wait(&f->cond, &f->lock);
            lock_release(&f->lock);
            // And return the slot the sector was read into (no need for slot_of!)
            A_INC(cache_v[f->slot].in_use);
            return f->slot;
        }
    }
    // No one is bringing the sector in yet.
    // Set up caching_in_elem
    caching_in_elem *elem = malloc(sizeof(caching_in_elem));
    lock_init(&elem->lock);
    cond_init(&elem->cond);
    elem->sector = sector;
    list_push_back(&caching_in_list, &elem->elem);
    lock_release(&caching_in_list_lock);

    // It is possible that two processes tried to bring in the same sector, both
    // discovered that it's not cached in, and then one starts and finishes bringing
    // it in before the other iterates through caching_in_list. This slot_of makes
    // sure that doesn't result in bringing the sector in twice
    int new_slot_try2 = slot_of(sector);
    if (new_slot_try1 != -1)
    {
        lock_acquire(&caching_in_list_lock);
        elem->slot = new_slot_try2;
        // In case someone saw the elem in the list and waited on the condition
        lock_acquire(&elem->lock);
        cond_broadcast(&elem->cond, &elem->lock);
        lock_release(&elem->lock);
        list_remove(&elem->elem);
        lock_release(&caching_in_list_lock);
        free(elem);
        return new_slot_try1;
    }

    // Otherwise we're free to read in the sector
    int new_slot = evict_slot();
    read_into_slot(new_slot, sector);
    // Now remove elem from caching_in_list and wake up everyone that was conditioned on it
    lock_acquire(&caching_in_list_lock);
    elem->slot = new_slot;
    lock_acquire(&elem->lock);
    cond_broadcast(&elem->cond, &elem->lock);
    lock_release(&elem->lock);
    list_remove(&elem->elem);
    lock_release(&caching_in_list_lock);
    free(elem);

    // Also, add next slot to read_ahead_list
    block_sector_t last_sector = byte_to_sector(inode, inode_length(inode));
    if (not_read_ahead && sector < last_sector)
    {
        read_ahead_elem *ra_elem = malloc(sizeof(read_ahead_elem));
        ra_elem->sector = sector + 1;
        ra_elem->inode = inode;
        lock_acquire(&read_ahead_lock);
        list_push_back(&read_ahead_list, &ra_elem->elem);
        lock_release(&read_ahead_lock);
    }
    return new_slot;
}


// I'm a little uncomfortable just moving all the inode read write code over here
// Seems like there should be a neater way of doing it, but I want the cache to
// stay static

// TODO when file extension happens, need to be careful reading between the
// old EOF and the new EOF. Writes past EOF must be atomic with respect to reads 
// past EOF
off_t cache_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;

    while (size > 0) {
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector (inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;
        /* The cached data for the sector to read */
        int slot = get_cache_slot(sector_idx, inode, true);
        cache_v[slot].last_used = timer_ticks();

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
            /* Read full sector directly into caller's buffer. */
            memcpy (buffer + bytes_read, cache_nv[slot].data, chunk_size);
        }
        else {
            memcpy(buffer + bytes_read, cache_nv[slot].data + sector_ofs, chunk_size);
        }
        A_DEC(cache_v[slot].in_use);

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }
    return bytes_read;
}


// TODO deal with writes past EOF
off_t cache_write_at(struct inode *inode, const void *buffer_, off_t size, off_t offset)
{
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;

    if (writes_forbidden(inode))
        return 0;

    while (size > 0) {
        /* Sector to write, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;
        /* The cached data for the sector to read */
        int slot = get_cache_slot(sector_idx, inode, true);
        cache_v[slot].last_used = timer_ticks();
        cache_v[slot].dirty = true;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
            /* Write full sector directly to disk. */
            memcpy (cache_nv[slot].data, buffer + bytes_written, chunk_size);
        }
        else {
            memcpy(cache_nv[slot].data + sector_ofs, buffer + bytes_written, chunk_size);
        }
        A_DEC(cache_v[slot].in_use);

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }
    return bytes_written;
}
