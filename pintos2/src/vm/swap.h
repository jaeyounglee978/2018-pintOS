#ifndef __SWAP__
#define __SWAP__

#include "threads/thread.h"
#include "devices/block.h"


struct swap_entry_list;
struct swap_entry;

void swap_disk_init(void);

block_sector_t swap_find_empty_sector();
struct swap_entry *swap_entry_insert(const void *kpage, const tid_t owner_pid);

void swap_frame_to_disk(const void *kpage, const tid_t owner_pid);
void swap_disk_to_frame(const void* dst, const void *kpage, const tid_t owner_pid);

struct swap_entry *swap_entry_lookup(const void *kpage, const tid_t owner_pidd);

#endif
