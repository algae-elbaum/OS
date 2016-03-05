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

Note that accesses to the suppl_page table can be done concurrently, however
any modifications need to hold a lock of some kind. There are a bunch of choices
for what type of lock to use. For example, we could make each particular part 
of the table have its own write-lock this would allow for simaltaneous reads
and writes. However, having this many locks would likely harm system
resources and actually take longer. It is also more complex and error-prone
though it would be an interesting idea to explore in a future project.
Thus, we will make a simple lock system. This will likely cause massive slow
downs, but just locking the whole table is safe. IF time allows, change this 
system so that we can get concurrent reads.
*/

#include <debug.h>
#include "page.h"

struct hash suppl_page_table;

/* To insert values into the page table we want

hash_insert (&pages, &p->hash_elem);

Since this is a hash we can assert that there are no collisions
but if we want to check for that we need hash_find. 
It is for that reason that we need the suppl_page_less function
because we need an ordering on the suppl_page objects */

/* Returns a hash value for page p. */
unsigned suppl_page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
	const struct suppl_page *p = hash_entry (p_, struct suppl_page, hash_elem);
	return hash_bytes (&p->vaddr, sizeof p->vaddr);
}

/* Returns true if page a precedes page b. */
bool suppl_page_less (const struct hash_elem *a_, const struct hash_elem *b_,
					 void *aux UNUSED)
{
	const struct suppl_page *a = hash_entry (a_, struct suppl_page, hash_elem);
	const struct suppl_page *b = hash_entry (b_, struct suppl_page, hash_elem);

	return a->vaddr < b->vaddr;
}

void init_suppl_page_table(void)
{
    hash_init (&suppl_page_table, suppl_page_hash, suppl_page_less, NULL);
}

/* Returns the page containing the given virtual address,
	 or a null pointer if no such page exists. */
struct suppl_page * suppl_page_lookup (const void *address)
{
	struct suppl_page p;
	struct hash_elem *e;

    // Gotta hash with the thread in addition to the page somehow, since virtual
    // page addresses overlap all the time between user processes. Is this what 
    // aux is for?
    p.holding_thread = thread_current();
	p.vaddr = address;
	e = hash_find (&suppl_page_table, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct suppl_page, hash_elem) : NULL;
}

// For evicting a page. If the page is dirty, write it out to a file or swap
// slot as appropriate. Use the suppl page entry to figure out what's needed
bool write_out_page(void *page UNUSED)
{
	return false;
}
