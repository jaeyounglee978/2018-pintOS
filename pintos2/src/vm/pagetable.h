#ifndef __S_PAGETABLE__
#define __S_PAGETABLE__

#include <stdbool.h>
#include "threads/thread.h"

struct page_entry;

void pagetable_init(void);
void *page_entry_upage(struct page_entry *p);
void *page_entry_kpage(struct page_entry *p);
bool page_entry_writable(struct page_entry *p);
void page_entry_update_kpage(struct page_entry *p, void *new_kpage);

struct page_entry *page_entry_lookup(const void *upage, tid_t owner_pid);
struct page_entry *page_entry_insert(const void *upage, const void *kpage, const bool writable, tid_t owner_id);
bool is_swapped(struct page_entry *p);
bool flap_swapped_flag(struct page_entry *p);

void *page_swap_to_disk(void);

#endif
