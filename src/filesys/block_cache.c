#include "block_cache.h"
#include "devices/timer.h"
#include "devices/block.h"
#include <debug.h>

#define NUM_SLOTS 64
//#define MAX_SPACE (NUM_SLOTS * BLOCK_SECTOR_SIZE)
#define A_INC(X) {__sync_add_and_fetch (&(X), 1);};  // Amazing
#define A_DEC(X) {__sync_add_and_fetch (&(X), -1);}; // Amazing

typedef struct cache_slot_v // v for volatile
{
    int32_t block_id;                       // Address of block
    volatile int in_use;                    // Count processes using this entry
    volatile int64_t last_used;             // Holds the tick when this was used last
    volatile int trying_to_evict;           /* So that nothing tries to use the block too 
                                       	       late into eviction to stop the eviction */
} cache_slot_v;

// Separate struct because volatility is apparently infections
// For the first time in a long time I'm seriously annoyed with C
typedef struct cache_slot_nv // nv for not volatile
{
    char data[BLOCK_SECTOR_SIZE];  // Actual data
    struct lock block_id_lock;    // For finding free slots
    struct lock in_use_lock;      // lock for in_use and trying_to_evict
} cache_slot_nv;

//static uint32_t free_slots = NUM_SLOTS;
static volatile cache_slot_v cache_v[NUM_SLOTS] = {{0}};
static cache_slot_nv cache_nv[NUM_SLOTS];

// Write slot back to disk out and clear out the struct's fields
static void write_out_slot(int slot UNUSED)
{
	// Do the actual writing
}

// Read the block into the slot and fill in the struct's fields
static void read_into_slot(int slot UNUSED, uint32_t block_id UNUSED)
{

}

// Simultaneous calls may chase each other around for a bit due to the
// trying_to_evict check until they get out of synch and one inc of
// trying_to_evict happens early enough that the other call has yet
// to hit the first if in the for  
static int find_slot_to_evict(void)
{
	int64_t best_time = timer_ticks();
	int best = -1;
	// 'while' to make sure that at the end nothing grabbed the evictee for use
	while (best != -1 && cache_v[best].in_use != 0 && cache_v[best].trying_to_evict == 1)
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
	// Could do this faster with a count of free slots, but... TODO maybe later
	for (i = 0; i < NUM_SLOTS; i++)	
	{
	    lock_acquire(&cache_nv[i].block_id_lock);
		if (cache_v[i].block_id == -1)
		{
			cache_v[i].block_id = 0;
	 	    lock_release(&cache_nv[i].block_id_lock);
			return i;
		}	    
 	    lock_release(&cache_nv[i].block_id_lock);
    }
    int free_slot = find_slot_to_evict();
    write_out_slot(free_slot);
    return free_slot;
}


//TODO Two processes may not bring in the same block separately (locked list of blocks being currently processed?)
void *get_cache_data(int32_t block_id)
{
	int i;
	// It will be possible for two processes asking for the same block at the
	// same time to read in the block
	for (i = 0; i < NUM_SLOTS; i++)
	{
	    // Prevent the slot from being evicted
	    A_INC(cache_v[i].in_use);
	    if (!cache_v[i].trying_to_evict && cache_v[i].block_id == block_id)
	    {
	    	cache_v[i].last_used = timer_ticks();
	    	return cache_nv[i].data;
	    }
	    // Not the slot we want, current thread should no longer block eviction
	    A_DEC(cache_v[i].in_use);
    }

    // block is not cached, bring it in
    int new_slot = evict_slot();
    read_into_slot(new_slot, block_id);
    return cache_nv[new_slot].data;
}