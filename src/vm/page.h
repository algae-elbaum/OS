#include <hash.h>
#include "threads/thread.h"

typedef struct suppl_page
{
   /* We have some extra information that we need to keep track of for 
      each page that we use. */
   const struct thread *holding_thread; 
   bool read_only;
   const void *vaddr; //virtual address
   const void *paddr;
   int size; // in bytes <<< of what?
   char* file_name; // If we want to use the anonymous file, then we can call it
                    // "" because files shoudn't be just named the empty string
   unsigned file_offset;
   struct hash_elem hash_elem; 
// suppl_pages need to be part of a hash table so that we can get things out
} suppl_page;

void init_suppl_page_table(void);

// For eviction
bool write_out_page(void *page);

suppl_page *suppl_page_lookup(const void *page);



/**** Is there a reason to make these hashing functions visible to other files? ****/
unsigned suppl_page_hash (const struct hash_elem *p_, void *aux UNUSED);

bool suppl_page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED);
