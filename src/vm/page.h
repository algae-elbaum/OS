#include <hash.h>

/**** Is there a reason to make the suppl page and hashing visible to other files? ****/

struct suppl_page
{
   /* We have some extra information that we need to keep track of for 
      each page that we use. */
   bool read_only;
   int user_virtual_addr;
   int physical_addr;
   int size; // in bytes
   char* file_name; // If we want to use the anonymous file, then we can call it
                    // '\0' because files shoudn't be just named the empty string  
   struct hash_elem hash_elem; 
// suppl_pages need to be part of a hash table so that we can get things out
};

unsigned suppl_page_hash (const struct hash_elem *p_, void *virtual_mem UNUSED);

bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED);

// For eviction
bool write_out_page(void *page);

/* 	
Now we can manipulate the hash table we've created. If p is a pointer to a struct page, we can insert it into the hash table with:
 	
hash_insert (&pages, &p->hash_elem);
If there's a chance that pages might already contain a page with the same addr, then we should check hash_insert()'s return value.
To search for an element in the hash table, use hash_find(). This takes a little setup, because hash_find() takes an element to compare against. Here's a function that will find and return a page based on a virtual address, assuming that pages is defined at file scope:

struct page is allocated as a local variable here on the assumption that it is fairly small. Large structures should not be allocated as local variables. See section A.2.1 struct thread, for more information.
A similar function could delete a page by address using hash_delete().
A.8.6 Auxiliary Data

In simple cases like the example above, there's no need for the aux parameters. In these cases, just pass a null pointer to hash_init() for aux and ignore the values passed to the hash function and comparison functions. (You'll get a compiler warning if you don't use the aux parameter, but you can turn that off with the UNUSED macro, as shown in the example, or you can just ignore it.)
aux is useful when you have some property of the data in the hash table is both constant and needed for hashing or comparison, but not stored in the data items themselves. For example, if the items in a hash table are fixed-length strings, but the items themselves don't indicate what that fixed length is, you could pass the length as an aux parameter.
A.8.7 Synchronization

The hash table does not do any internal synchronization. It is the caller's responsibility to synchronize calls to hash table functions. In general, any number of functions that examine but do not modify the hash table, such as hash_find() or hash_next(), may execute simultaneously. However, these function cannot safely execute at the same time as any function that may modify a given hash table, such as hash_insert() or hash_delete(), nor may more than one function that can modify a given hash table execute safely at once.
It is also the caller's responsibility to synchronize access to data in hash table elements. How to synchronize access to this data depends on how it is designed and organized, as with any other data structure.
*/