/*
The supplemental page table is used for at least two purposes. Most importantly, 
on a page fault, the kernel looks up the virtual page that faulted in the 
supplemental page table to find out what data should be there. Second, the 
kernel consults the supplemental page table when a process terminates, to decide 
what resources to free.

You may organize the supplemental page table as you wish. There are at least two 
basic approaches to its organization: in terms of segments or in terms of pages. 
Optionally, you may use the page table itself as an index to track the members of 
the supplemental page table. You will have to modify the Pintos page table 
implementation in pagedir.c to do so. We recommend this approach for advanced 
students only. See section A.7.4.2 Page Table Entry Format, for more information.
*/

#include <debug.h>
#include "page.h"

// Let's get this hashing working next, but for now I want this to compile
#if 0
/* To set up the supplpage table later
struct hash suppl_page_table;

hash_init (&suppl_page_table, page_hash, page_less, NULL);*/
/* Returns a hash value for page p. */
unsigned suppl_page_hash (const struct hash_elem *p_, void *virtual_mem UNUSED)
{
  const struct suppl_page *p = hash_entry (p_, struct suppl_page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool suppl_page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct suppl_page *a = hash_entry (a_, struct suppl_page, hash_elem);
  const struct suppl_page *b = hash_entry (b_, struct suppl_page, hash_elem);

  return a->addr < b->addr;
}

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct suppl_page * suppl_page_lookup (const void *address)
{
  struct suppl_page p;
  struct hash_elem *e;

  p.addr = address;
  e = hash_find (&suppl_page_table, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct suppl_page, hash_elem) : NULL;
}
#endif

// For evicting a page. If the page is dirty, write it out to a file or swap
// slot as appropriate. Use the suppl page entry to figure out what's needed
bool write_out_page(void *page)
{
  return false;
}