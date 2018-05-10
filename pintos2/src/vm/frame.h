#ifndef __FRAME__
#define __FRAME__

#include "threads/thread.h"

struct frame_entry;

void frametable_init(void);

void *frame_entry_upage(struct frame_entry *f);
void *frame_entry_kpage(struct frame_entry *f);
void *frame_entry_owner_pid(struct frame_entry *f);
struct frame_entry *frame_entry_insert(const void *upage, const void *kpage, const tid_t owner_id);
struct frame_entry *frame_entry_lookup(const void *kpage, const tid_t owner_id);
void frame_entry_delete(struct frame_entry *f);
void frame_delete_by_pid(const tid_t tid);
struct frame_entry *find_swap_victim();

#endif