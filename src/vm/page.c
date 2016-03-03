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