#pragma once

#include <hash.h>

typedef struct suppl_page
{
   /* We have some extra information that we need to keep track of for 
      each page that we use. */
   bool read_only;
   const void *vaddr; //virtual address
   const void *paddr;
   int swap_index; // Must be initialized to -1 when a new suppl_page is made
   char* file_name; // If we want to use the anonymous file, then we can call it NULL
   unsigned file_offset;
   unsigned bytes_to_read;
   struct hash_elem hash_elem; 
// suppl_pages need to be part of a hash table so that we can get things out
} suppl_page;

// For eviction
bool write_out_page(void *page);

suppl_page *new_suppl_page(bool read_only, void *vaddr, void *paddr, char* file_name, 
                                unsigned file_offset, unsigned bytes_to_read);

suppl_page * suppl_page_lookup (struct hash *suppl_page_table, const void *address);

unsigned suppl_page_hash (const struct hash_elem *p_, void *aux UNUSED);

bool suppl_page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED);
