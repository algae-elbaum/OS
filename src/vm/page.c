/*
The supplemental page table is used for at least two purposes. Most importantly,
on a page fault, the kernel looks up the virtual page that faulted in the
supplemental page table to find out what data should be there. Second, the
kernel consults the supplemental page table when a process terminates, to decide
what resources to free.

Note that we have a different suppl_page table per process.

Note that accesses to the suppl_page table can be done concurrently, however
any modifications need to hold a lock of some kind. There are a bunch of choices
for what type of lock to use. For example, we could make each particular page
of the table have its own write-lock this would allow for simaltaneous reads
and writes. However, having this many locks would likely harm system
resources and actually take longer. It is also more complex and error-prone
though it would be an interesting idea to explore in a future project.
Thus, we will make a simple lock system. This will likely cause massive slow
downs, but just locking that table is safe. If time allows, change this
system so that we can get concurrent reads.
*/

#include <debug.h>
#include <string.h>
#include "page.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "threads/synch.h"

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

/* Returns the page containing the given virtual address,
         or a null pointer if no such page exists. */
suppl_page * suppl_page_lookup (struct hash *suppl_page_table, const void *vaddr)
{
    struct suppl_page p;
    struct hash_elem *e;

    p.vaddr = vaddr;
    e = hash_find (suppl_page_table, &p.hash_elem);
    return e != NULL ? hash_entry (e, struct suppl_page, hash_elem) : NULL;
}


suppl_page *new_suppl_page(bool read_only, void *vaddr)
{
    suppl_page *new_suppl_page = (suppl_page *) malloc(sizeof(suppl_page));
    new_suppl_page->read_only = read_only;
    new_suppl_page->vaddr = (void *) (((uintptr_t) vaddr >> PGBITS) << PGBITS);

    // Defaults:
    new_suppl_page->swap_index = -1;
    new_suppl_page->file_name[0] = '\0';
    new_suppl_page->file_offset = 0;
    new_suppl_page->bytes_to_read = 0;
    return new_suppl_page;
}

void set_suppl_page_file(suppl_page* page, char file_name[14], unsigned ofs, 
                            unsigned bytes_to_read)
{
    if (file_name != NULL)
        memcpy(page->file_name, file_name, 14);
    else
        page->file_name[0] = '\0';
    page->file_offset = ofs;
    page->bytes_to_read = bytes_to_read;
}
