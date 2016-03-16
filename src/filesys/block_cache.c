#include "block_cache.h"
#include <debug.h>
#include <string.h>
#include "threads/synch.h"
#include "devices/timer.h"
#include "devices/block.h"
#include "inode.h"
#include "filesys.h"

#define NUM_SLOTS 64
#define A_INC(X) {__sync_add_and_fetch (&(X), 1);};  // Amazing
#define A_DEC(X) {__sync_add_and_fetch (&(X), -1);}; // Amazing

typedef struct cache_slot_v // v for volatile
{
    volatile struct inode *inode;   // Associated inode
    volatile block_sector_t sector; // Sector held in the slot         
    volatile int in_use;            // Count processes using this entry
    volatile int64_t last_used;     // Holds the tick when this was used last
    volatile int trying_to_evict;   /* So that nothing tries to use the block too
                                       late into eviction to stop the eviction */
    volatile bool touched;          // Whether the slot has ever been used
} cache_slot_v;

// Separate struct because volatility is apparently infectious
// For the first time in a long time I'm seriously annoyed with C
typedef struct cache_slot_nv // nv for not volatile
{
    uint8_t data[BLOCK_SECTOR_SIZE];  // Actual data (seems like it should be volatile, but memcpy...)
} cache_slot_nv;

static volatile cache_slot_v cache_v[NUM_SLOTS] = {{0}};;
static cache_slot_nv cache_nv[NUM_SLOTS];

static volatile block_sector_t free_slots = NUM_SLOTS;

void cache_init(void)
{
    // int i;
    // for (i = 0; i < NUM_SLOTS; i++)
    // {
    //     int j;
    //     for (j = 0; j < BLOCK_SECTOR_SIZE; j++)
    //         cache_nv[i].data[j] = 1;
    // }
}

// TODO potentially keep track of whether slots are dirty and only then do any writing
static void write_out_slot(int slot)
{
    // Write out the entire sector. Files don't overlap sectors, so even if it
    // would matter, nothing past EOF will even be changed from when the slot 
    // was read in
    block_write(fs_device, cache_v[slot].sector, cache_nv[slot].data);
    // No need to clear out slot fields, if they need changing read_into_slot
    // will take care of it, and the periodic flushing of cache data shouldn't
    // affect anything but the disk
}

// Read the block into the slot and fill in the struct's fields
static void read_into_slot(int slot, struct inode *inode, block_sector_t sector)
{
    // Copy in the whole sector, even if past EOF. 
    // The logic for not reading those needs to happen elsewhere anyways.
    block_read (fs_device, sector, cache_nv[slot].data);
    cache_v[slot].sector = sector;
    // After reading in, the slot is given right to whatever uses it, so set in_use now
    cache_v[slot].in_use = 1;
    // If the slot was in use before, write_out_slot didn't clear trying_to_evict.
    // (don't want another eviction grabbing the slot and evicting again)
    cache_v[slot].trying_to_evict = 0;
    // Once this is set get_cache_data can find it, so must set at the end.
    cache_v[slot].inode = inode;
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
    if (best == -1)
        PANIC("AHHHHHHH AHHHHH AHHHHHHHHHHHHHHHHHHHH");
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


//TODO Two processes may not bring in the same block separately 
// (locked list of blocks being currently processed, plus conditions for newcomers
// to wait on)
static int get_cache_slot(struct inode *inode, block_sector_t sector)
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

    // block is not cached, bring it in
    int new_slot = evict_slot();
    read_into_slot(new_slot, inode, sector);
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
        int slot = get_cache_slot(inode, sector_idx);
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
        int slot = get_cache_slot(inode, sector_idx);
        cache_v[slot].last_used = timer_ticks();

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