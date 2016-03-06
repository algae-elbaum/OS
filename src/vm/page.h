#pragma once

#include <hash.h>

typedef struct suppl_page
{
   /* We have some extra information that we need to keep track of for 
      each page that we use. */
   bool read_only;
   const void *vaddr; // user virtual address of the page
   const void *kaddr;
   int swap_index; // Must be initialized to -1 when a new suppl_page is made
   char file_name[14]; // If we want to use the anonymous file, then we can call it ""
   unsigned file_offset;
   unsigned bytes_to_read;
   struct hash_elem hash_elem; 
// suppl_pages need to be part of a hash table so that we can get things out
   struct list_elem elem; // Pages are part of maps and we need to hold onto
   // which ones belong to which maps. 
} suppl_page;

// For eviction
bool write_out_page(void *page);

suppl_page *new_suppl_page(bool read_only, void *vaddr, void *kaddr, char *file_name, 
                                unsigned file_offset, unsigned bytes_to_read);

suppl_page * suppl_page_lookup(struct hash *suppl_page_table, const void *vaddr);

unsigned suppl_page_hash(const struct hash_elem *p_, void *aux UNUSED);

bool suppl_page_less(const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED);
