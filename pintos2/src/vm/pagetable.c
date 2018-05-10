#include <stdbool.h>
#include "lib/kernel/list.h"
#include "pagetable.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/synch.h"

struct page_entry_list
{
  struct list_elem list_elem;
  struct list entry_list;
  tid_t owner_pid;
};

struct page_entry
{
  struct list_elem list_elem;
  void *upage;
  void *kpage;
  bool writable;
  bool is_swapped;
  tid_t owner_pid;
};

struct list pagetable;
struct lock page_lock;

struct page_entry_list *page_entry_list_lookup(tid_t tid);

void
pagetable_init(void)
{
  lock_init(&page_lock);
  list_init(&pagetable);
  #define _PAGE_TABLE_VALID_
}


void *page_entry_upage(struct page_entry *p)
{
  return p->upage;
}

void *page_entry_kpage(struct page_entry *p)
{
  return p->kpage;
}

bool page_entry_writable(struct page_entry *p)
{
  return p->writable;
}

void page_entry_update_kpage(struct page_entry *p, void *new_kpage)
{
  p->kpage = new_kpage;
}

struct page_entry_list *
page_entry_list_lookup(tid_t owner_pid)
{
  struct list_elem *e;
  e = list_begin(&pagetable);

  for (; e != list_end(&pagetable); e = list_next(e))
  {
    struct page_entry_list *pe = list_entry(e, struct page_entry_list, list_elem);

    if(pe->owner_pid == owner_pid)
      return pe;
  }

  return NULL;
}

struct page_entry *
page_entry_lookup(const void *upage, tid_t owner_pid)
{
  struct page_entry_list *pel = page_entry_list_lookup(owner_pid);

  if (pel == NULL)
    return NULL;

  struct list_elem *e;
  e = list_begin(&pel->entry_list);

  for (; e != list_end(&pel->entry_list); e = list_next(e))
  {
    struct page_entry *pe = list_entry(e, struct page_entry, list_elem);

    if(pe->upage == upage)
      return pe;
  }

  return NULL;
}

struct page_entry *
page_entry_insert(const void *upage, const void *kpage, const bool writable, tid_t owner_pid)
{
  if (page_entry_lookup(upage, owner_pid) != NULL)
    return;

  struct page_entry_list *pel = page_entry_list_lookup(owner_pid);

  if (pel == NULL)
  {
    struct page_entry_list *new_pel = (struct page_entry_list *)malloc(sizeof(struct page_entry_list));
    new_pel->owner_pid = owner_pid;
    list_init(&new_pel->entry_list);
    list_push_back(&pagetable, &new_pel->list_elem);
    pel = new_pel;
  }

  struct page_entry *new_page_entry = (struct page_entry *)malloc(sizeof(struct page_entry));

  new_page_entry->upage = upage;
  new_page_entry->kpage = kpage;
  new_page_entry->writable = writable;
  new_page_entry->is_swapped = false;
  new_page_entry->owner_pid = owner_pid;

  list_push_back(&pel->entry_list, &new_page_entry->list_elem);

  return new_page_entry;
}

struct list_elem *
page_entry_delete(struct page_entry *p)
{
  struct list_elem *h = list_remove(&p->list_elem);
  return h;
}

bool
is_swapped(struct page_entry *p)
{
  if (p == NULL)
    return false;
  return p->is_swapped;
}

bool
flap_swapped_flag(struct page_entry *p)
{
  p->is_swapped = !p->is_swapped;
  return p->is_swapped;
}

void *
page_swap_to_disk(void)
{
  lock_acquire(&page_lock);
  //printf("  PAGE SWAP DISK\n");
  void *new_page;
  struct frame_entry *f_e = find_swap_victim();
  struct page_entry *p_e = page_entry_lookup(frame_entry_upage(f_e), frame_entry_owner_pid(f_e));


  //printf("prev-allocated page = %p, victim frame = %p\n", frame_entry_upage(f_e), frame_entry_kpage(f_e));
  //printf("page_entry(%p), upage = %p, kpage = %p\n", p_e, p_e->upage, p_e->kpage);

  ASSERT(f_e != NULL);
  ASSERT(p_e != NULL);
  ASSERT(!is_swapped(p_e));
  //ASSERT(frame_entry_kpage(f_e) == p_e->kpage);

  //printf("validity : OK\n");

  new_page = p_e->kpage;

  if(get_thread_by_tid(p_e->owner_pid) != NULL)
  {
    pagedir_clear_page(get_thread_by_tid(p_e->owner_pid)->pagedir, p_e->upage);
  }

  flap_swapped_flag(p_e);
  swap_frame_to_disk(frame_entry_kpage(f_e), p_e->owner_pid);
  frame_entry_delete(f_e);


  memset(new_page, 0, PGSIZE);

  ASSERT(new_page != NULL);

  //printf("new allocated frame = %p\n", new_page);

  lock_release(&page_lock);
  return new_page;
}