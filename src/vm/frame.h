#include "threads/thread.h"

void *get_unused_frame(struct thread *holding_thread, void *upage);
void evict_thread_frames(struct thread *t);