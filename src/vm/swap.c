/*
The swap table tracks in-use and free swap slots. It should allow picking an 
unused swap slot for evicting a page from its frame to the swap partition. It 
should allow freeing a swap slot when its page is read back or the process 
whose page was swapped is terminated.
You may use the BLOCK_SWAP block device for swapping, obtaining the struct 
block that represents it by calling block_get_role(). From the vm/build 
directory, use the command pintos-mkdisk swap.dsk --swap-size=n to create an 
disk named swap.dsk that contains a n-MB swap partition. Afterward, swap.dsk 
will automatically be attached as an extra disk when you run pintos. 
Alternatively, you can tell pintos to use a temporary n-MB swap disk for a single run with --swap-size=n.
Swap slots should be allocated lazily, that is, only when they are actually required by eviction. Reading data pages from the executable and writing them to swap immediately at process startup is not lazy. Swap slots should not be reserved to store particular pages.
Free a swap slot when its contents are read back into a frame.
*/