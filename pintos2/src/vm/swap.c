#include <stdbool.h>
#include "swap.h"
#include "devices/block.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "threads/synch.h"

struct swap_entry_list
{
  struct list_elem list_elem;
  tid_t owner_pid;
  struct list swap_list;
};

struct swap_entry
{
  struct list_elem list_elem;
  void *kpage;
  block_sector_t disk_offset;
};


struct block *swap_disk;
struct list swap_table;
block_sector_t swap_disk_size;
bool *empty_block_sector;
void *disk_buffer;
struct lock swap_lock;


struct swap_entry *swap_entry_lookup_sub(const void *kpage, struct list *l);
struct swap_entry_list *swap_entry_list_lookup(const tid_t owner_id);


void
swap_disk_init(void)
{
  list_init(&swap_table);
  lock_init(&swap_lock);
  swap_disk = block_get_role(BLOCK_SWAP);
  swap_disk_size = block_size(swap_disk) / 8;

  empty_block_sector = (bool *)malloc(swap_disk_size);
  int i = 0;
  for (; i < swap_disk_size; i++)
    empty_block_sector[i] = true;

  disk_buffer = malloc(BLOCK_SECTOR_SIZE);
}

block_sector_t
swap_find_empty_sector()
{
  int i;
  for (i = 0; i < swap_disk_size; i++)
    if (empty_block_sector[i] == true)
    {
      empty_block_sector[i] = false;
      return (block_sector_t)i;
    }

  return swap_disk_size + 1;
}

struct swap_entry *
swap_entry_insert(const void *kpage, const tid_t owner_pid)
{
  block_sector_t empty_sector = swap_find_empty_sector();

  ASSERT(empty_sector < swap_disk_size);

  struct swap_entry_list *sl = swap_entry_list_lookup(owner_pid);

  if (sl == NULL)
  {
    struct swap_entry_list *new_swap_entry_list = (struct swap_entry_list *)malloc(sizeof(struct swap_entry_list));
    new_swap_entry_list->owner_pid = owner_pid;
    list_init(&new_swap_entry_list->swap_list);
    list_push_back(&swap_table, &new_swap_entry_list->list_elem);

    sl = new_swap_entry_list;
  }

  struct swap_entry *new_swap_entry = (struct swap_entry *)malloc(sizeof(struct swap_entry));
  new_swap_entry->kpage = kpage;
  new_swap_entry->disk_offset = empty_sector;
  list_push_back(&sl->swap_list, &new_swap_entry->list_elem);
  
  return new_swap_entry;
}


void
swap_frame_to_disk(const void *kpage, const tid_t owner_pid)
{
  lock_acquire(&swap_lock);
  struct swap_entry *s = swap_entry_insert(kpage, owner_pid);
  //printf("(%s, %d)  SWAP(f->d) : swap_entry %p, swap offset %d\n", thread_current()->name, thread_current()->tid, s, s->disk_offset);
  //printf("(%s, %d)  SWAP(f->d) : from %p, to disk\n", thread_current()->name, thread_current()->tid, kpage);

  if (kpage == 0xc029d000 && owner_pid == 4)
  {
    hex_dump((uint32_t)kpage, kpage, 16, true);
    hex_dump((uint32_t)kpage+16, kpage+16, 16, true);
    hex_dump((uint32_t)kpage+32, kpage+32, 16, true);
    hex_dump((uint32_t)kpage+48, kpage+48, 16, true);
  }

  int i;
  for (i = 0; i < 8; i++)
  {
    memcpy(disk_buffer, kpage + (i * BLOCK_SECTOR_SIZE), BLOCK_SECTOR_SIZE);
    block_write (swap_disk, (8 * s->disk_offset) + i, disk_buffer); 
  }
  lock_release(&swap_lock);
  //printf("swap_end\n");
}


void
swap_disk_to_frame(const void *dst, const void *kpage, const tid_t owner_pid)
{
  lock_acquire(&swap_lock);
  struct swap_entry *s = swap_entry_lookup(kpage, owner_pid);
  //printf("(%s, %d)  SWAP(d->f) : swap_entry %p, swap offset %d\n", thread_current()->name, thread_current()->tid, s, s->disk_offset);
  //printf("(%s, %d)  SWAP(d->f) : from disk, to %p\n", thread_current()->name, thread_current()->tid, dst);

  if (s == NULL)
    return;

  int i;
  for (i = 0; i < 8; i++)
  {
    block_read (swap_disk, (8 * s->disk_offset) + i, disk_buffer); 
    memcpy(dst + (i * BLOCK_SECTOR_SIZE), disk_buffer, BLOCK_SECTOR_SIZE);
  }

  swap_entry_delete(s);
  lock_release(&swap_lock);
  //printf("swap_end\n");
}


struct swap_entry_list *
swap_entry_list_lookup(const tid_t owner_id)
{
  struct list_elem *e;
  e = list_begin(&swap_table);

  for (; e != list_end(&swap_table); e = list_next(e))
  {
    struct swap_entry_list *se = list_entry(e, struct swap_entry_list, list_elem);

    if (se->owner_pid == owner_id)
      return se;
  }

  return NULL;
}


struct swap_entry *
swap_entry_lookup_sub(const void *kpage, struct list *l)
{
  struct list_elem *e = list_begin(l);

  for (; e != list_end(l); e = list_next(e))
  {
    struct swap_entry *s = list_entry(e, struct swap_entry, list_elem);

    if (s->kpage == kpage)
      return s;
  }

  return NULL;
}

struct swap_entry *
swap_entry_lookup(const void *kpage, const tid_t owner_pid)
{
  struct swap_entry_list *sl = swap_entry_list_lookup(owner_pid);

  return sl != NULL ? swap_entry_lookup_sub(kpage, &sl->swap_list) : NULL;
}

void
swap_entry_delete(struct swap_entry *s)
{
  empty_block_sector[s->disk_offset] = true;
  list_remove(s);
  free(s);
}

void
swap_entry_delete_by_tid(tid_t owner_pid)
{
  lock_acquire(&swap_lock);
  struct swap_entry_list *sl = swap_entry_list_lookup(owner_pid);

  if (sl != NULL)
  {
    list_remove(&sl->list_elem);
    struct list_elem *e = list_begin(&sl->swap_list);

    while(e != list_end(sl))
    {
      list_remove(e);
      free(list_entry(e, struct swap_entry, list_elem));
    }

    free(sl);
  }

  lock_release(&swap_lock);
  return;
}
