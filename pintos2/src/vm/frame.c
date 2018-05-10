#include <stdbool.h>
#include "frame.h"
#include "lib/kernel/list.h"
#include "threads/thread.h"

struct frame_entry
{
  struct list_elem list_elem;
  void *upage;
  void *kpage;
  tid_t owner_pid;
};

struct list frametable;


void
frametable_init(void)
{
  list_init(&frametable);
}


void *frame_entry_upage(struct frame_entry *f)
{
  return f->upage;
}
void *frame_entry_kpage(struct frame_entry *f)
{
  return f->kpage;
}
void *frame_entry_owner_pid(struct frame_entry *f)
{
  return f->owner_pid;
}

struct frame_entry *
frame_entry_insert(const void *upage, const void *kpage, const tid_t owner_pid)
{
  struct frame_entry *new_frame_entry = (struct frame_entry *)malloc(sizeof(struct frame_entry));

  new_frame_entry->upage = upage;
  new_frame_entry->kpage = kpage;
  new_frame_entry->owner_pid = owner_pid;

  list_push_back(&frametable, &new_frame_entry->list_elem);

  return new_frame_entry;
}

struct frame_entry *
frame_entry_lookup(const void *kpage, const tid_t owner_pid)
{
  struct list_elem *e;
  e = list_begin(&frametable);

  for (; e != list_end(&frametable); e = list_next(e))
  {
    struct frame_entry *f = list_entry(e, struct frame_entry, list_elem);

    if (f->owner_pid == owner_pid && f->kpage == kpage)
      return f;
  }

  return NULL;
}

void
frame_entry_delete(struct frame_entry *f)
{
  list_remove(&f->list_elem);
  free(f);
}

void
frame_delete_by_pid(const tid_t tid)
{
  struct thread *t = get_thread_by_tid(tid);
  struct list_elem *e = list_begin(&frametable);
  struct list_elem *list_end_elem = list_end(&frametable);

  while(e != list_end_elem)
  {
    struct frame_entry *f = list_entry(e, struct frame_entry, list_elem);
    struct list_elem *s = list_next(e);

    if (f->owner_pid == tid)
    {
      list_remove(e);
      free(f);
    }
    e = s;
  }

  return;
}

struct frame_entry *
find_swap_victim()
{
  return list_entry(list_pop_front(&frametable), struct frame_entry, list_elem);
}

