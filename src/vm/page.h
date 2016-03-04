#include <hash.h>

struct suppl_page
{
   /* We have some extra information that we need to keep track of for 
      each page that we use. */
   bool read_only;
   int addr; //virtual address
   int physical_addr;
   int size; // in bytes
   char* file_name; // If we want to use the anonymous file, then we can call it
                    // '\0' because files shoudn't be just named the empty string  
   struct hash_elem hash_elem; 
// suppl_pages need to be part of a hash table so that we can get things out
}

unsigned suppl_page_hash (const struct hash_elem *p_, void *virtual_mem UNUSED);

bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED);
