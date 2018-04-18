#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <string.h>
#include <list.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

static void syscall_handler (struct intr_frame *);
void syscall_halt (void);
void syscall_exit (int status);
pid_t syscall_exec (const char *file);
int syscall_wait (pid_t pid);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove (const char *file);
int syscall_open (const char *file);
int syscall_filesize (int fd);
int syscall_read (int fd, void *buffer, unsigned length);
int syscall_write (int fd, const void *buffer, unsigned length);
void syscall_seek (int fd, unsigned position);
unsigned syscall_tell (int fd);
void syscall_close (int fd);

struct lock syscall_lock;

enum FILE_STATE
{
  FILE_IDLE,
  FILE_OPEN,
  FILE_CLOSED
};


struct file_fd_name
{
	int tid;
	int fd;
	char *name;
	struct file *file;
	enum FILE_STATE file_state;
	struct list_elem elem;
};

static struct list fd_name_mapping;
int fd_index = 2;

struct file *find_file_by_fd(int fd);
struct file *find_file_by_name(char *f);
struct file_fd_name *find_mapping_by_name(const char *f);
struct file_fd_name *find_mapping_by_fd(int fd);
struct file_fd_name *find_mapping_by_tid(tid_t tid);
bool is_valid_addr(const void *addr);
void catch_addr_error(const void *addr);


struct file *
find_file_by_name(char *f)
{
  struct list_elem *e;
  e = list_begin(&fd_name_mapping);
  printf("e = %p\n", e);
  printf("find name\n");

  for (e; e != list_end(&fd_name_mapping); e = list_next(e))
  {
    struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
    if (strcmp(ffn->name, f) == 0)
      return ffn->file;
  }

  return NULL;
}

struct file *
find_file_by_fd(int fd)
{
  struct list_elem *e;
  e = list_begin(&fd_name_mapping);

  for (e; e != list_end(&fd_name_mapping); e = list_next(e))
  {
    struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
    if (ffn->fd == fd)
    {
      return ffn->file;
    }
  }

  return NULL;
}

struct file_fd_name *
find_mapping_by_name(const char *f)
{
	struct list_elem *e;
	e = list_begin(&fd_name_mapping);
  printf("");

	for (e; e != list_end(&fd_name_mapping); e = list_next(e))
	{
		struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
		if (strcmp(ffn->name, f) == 0)
			return ffn;
	}

	return NULL;
}

struct file_fd_name *
find_mapping_by_fd(int fd)
{
	struct list_elem *e;
	e = list_begin(&fd_name_mapping);

	for (e; e != list_end(&fd_name_mapping); e = list_next(e))
	{
		struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
		if (ffn->fd == fd)
			return ffn;
	}

	return NULL;
}

struct file_fd_name *
find_mapping_by_tid(tid_t tid)
{
  struct list_elem *e;
  e = list_begin(&fd_name_mapping);

  for (e; e != list_end(&fd_name_mapping); e = list_next(e))
  {
    struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
    if (ffn->tid == tid)
      return ffn;
  }

  return NULL;
}


void
syscall_init (void) 
{
  lock_init(&syscall_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&fd_name_mapping);
}

bool
is_valid_addr(const void *addr)
{
	if (is_user_vaddr(addr))
	{
		struct thread* t = thread_current();
		void *p = pagedir_get_page(t->pagedir, addr);
		if (p != NULL)
			return true;
		else
			return false;
	}
	else
		return false;
}

void
catch_addr_error(const void *addr)
{
	if (!is_valid_addr(addr))
		syscall_exit(-1);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	catch_addr_error(f->esp);
  int syscall_number = *(int*)f->esp;
  ////printf("current esp = %p\n", f->esp);
  ////printf("why syscall number = %d, %d\n", syscall_number, *(int *) f->esp);
  ////printf("  SYSCALL %d\n", syscall_number);

  switch(syscall_number)
  {
  	case SYS_HALT:
  	{
  	  syscall_halt();
  	  break;
  	}
  	case SYS_EXIT:
  	{
  		//printf("  SIZE = %d\n", list_size(&fd_name_mapping));
  	  catch_addr_error(f->esp + 4);
  	  int status = *(int*)(f->esp + 4);
  	  f->eax = status;
  	  syscall_exit(status);
  	  break;
  	}
  	case SYS_EXEC:
  	{
  	  void *file;
  	  catch_addr_error(f->esp + 4);
  	  file = *(void **)(f->esp + 4);
  	  pid_t pid = syscall_exec((const char *)file);
  	  f->eax = pid;
  	  break;
  	}
  	case SYS_WAIT:
  	{
  	  catch_addr_error(f->esp + 4);
  	  pid_t pid = *(pid_t*)(f->esp + 4);
  	  int i = syscall_wait(pid);
  	  f->eax = i;
  	  break;
  	}
  	case SYS_CREATE:
  	{
  	  catch_addr_error(f->esp + 8);
  	  void *file = *(void **)(f->esp + 4);
  	  unsigned initial_size = *(unsigned*)(f->esp + 8);
  	  bool b = syscall_create((const char *)file, initial_size);
  	  f->eax = b;
  	  break;
  	}
  	case SYS_REMOVE:
  	{
  	  void *file;
  	  catch_addr_error(f->esp + 4);
  	  file = *(void **)(f->esp + 4);
  	  bool b = syscall_remove((const char *)file);
  	  f->eax = b;

  	  break;
  	}
  	case SYS_OPEN:
  	{
  	  void *file;
  	  catch_addr_error(f->esp + 4);
  	  file = *(void **)(f->esp + 4);
  	  int i = syscall_open((const char *)file);
  	  f->eax = i;

  	  break;
  	}
  	case SYS_FILESIZE:
  	{
  	  catch_addr_error(f->esp + 4);
  	  int fd = *(int*)(f->esp + 4);
  	  int i = syscall_filesize(fd);
  	  f->eax = i;

  	  break;
  	}
  	case SYS_READ:
  	{
  	  catch_addr_error(f->esp + 12);
  	  int fd = *(int*)(f->esp + 4);
  	  uint32_t buffer_addr = *(void **)(f->esp + 8);
  	  unsigned length = *(unsigned*)(f->esp + 12);

  	  int i = syscall_read(fd, (void *)buffer_addr, length);
  	  f->eax = i;

  	  break;
  	}
  	case SYS_WRITE:
  	{
  		catch_addr_error(f->esp + 12);
  	  int fd = *(int*)(f->esp + 4);
  	  uint32_t buffer_addr = *(void **)(f->esp + 8);
  	  unsigned length = *(unsigned *)(f->esp + 12);

  	  int i = syscall_write(fd, (const void *)buffer_addr, length);
  	  f->eax = i;

  	  break;
  	}
  	case SYS_SEEK:
  	{
  		catch_addr_error(f->esp + 8);
  	  int fd = *(int*)(f->esp + 4);
  	  unsigned position = *(unsigned*)(f->esp + 8);

  	  syscall_seek(fd, position);

  	  break;
  	}
  	case SYS_TELL:
  	{
  	  catch_addr_error(f->esp + 4);
  	  int fd = *(int*)(f->esp + 4);

  	  unsigned u = syscall_tell(fd);
  	  f->eax = u;

  	  break;
  	}
  	case SYS_CLOSE:
  	{
  	  catch_addr_error(f->esp + 4);
  	  int fd = *(int*)(f->esp + 4);
  	  syscall_close(fd);
  	  break;
  	}
  	default:
  	  break;
  }
}



void syscall_halt (void)
{
  shutdown_power_off();
}

void syscall_exit (int status)
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_current()->exit_status = status;

  struct file_fd_name *ffn = find_mapping_by_tid(thread_current()->tid);
  while ((ffn = find_mapping_by_tid(thread_current()->tid)) != NULL)
  {
    list_remove(&ffn->elem);
    file_close(ffn->file);
    free(ffn); 
  }

  thread_exit();
}

pid_t syscall_exec (const char *file)
{
	catch_addr_error(file);
	catch_addr_error(file + strlen(file) - 1);

	lock_acquire(&syscall_lock);
	pid_t pid = process_execute(file);
	lock_release(&syscall_lock);

  return pid;
}

int syscall_wait (pid_t pid)
{
  int ret = process_wait(pid);
  return ret;
}

bool syscall_create (const char *file, unsigned initial_size)
{

  if (file == NULL)
  	syscall_exit(-1);

  if(!is_valid_addr((void *)file))
  {
  	syscall_exit(-1);
  }

  if(!strcmp(file, ""))
  	syscall_exit(-1);

  lock_acquire(&syscall_lock);
  bool create = filesys_create(file, initial_size);
  lock_release(&syscall_lock);

  return create;
}

bool syscall_remove (const char *file)
{
  //printf("file delete\n");
  lock_acquire(&syscall_lock);
  bool delete = filesys_remove(file);
  lock_release(&syscall_lock);

  return delete;
}

int syscall_open (const char *file)
{
  //printf("syscall_open(%s) : %p\n", thread_current()->name, file);

  if (file == NULL)
  {
  	syscall_exit(-1);
  }

  if(!is_valid_addr((void *)file))
  {
  	syscall_exit(-1);
  }

  if(!strcmp(file, ""))
  	return -1;

  int tid = thread_current()->tid;
  struct file_fd_name *ffd;
  ffd = (struct file_fd_name *)malloc(sizeof(struct file_fd_name));


  lock_acquire(&syscall_lock);

  struct file_fd_name *ffn;
  struct file *_file;


  ffn = find_mapping_by_name(file);

  /*if (ffn != NULL)
  {
    printf("file opened %p\n", ffn->file);
    _file = ffn->file;
    file_reopen(ffn->file);
  }
  else
  {*/
    _file = filesys_open(file);
    //printf("file newly opened %p\n", _file); 
  //}


  if(_file == NULL)
  {
  	free(ffd);
    lock_release(&syscall_lock);
  	return -1;	
  }

  //ffd = (struct file_fd_name *)malloc(sizeof(struct file_fd_name));

  ffd->tid = tid;
  ffd->name = file;
  ffd->file = _file;
  ffd->fd = fd_index;
  ffd->file_state = FILE_OPEN;
  fd_index++;
  list_push_back(&fd_name_mapping, &ffd->elem);

  lock_release(&syscall_lock);

  return ffd->fd;
}
int syscall_filesize (int fd)
{
  lock_acquire(&syscall_lock);
  struct file *file = find_file_by_fd(fd);

  if (file != NULL)
  {
    lock_release(&syscall_lock);
  	return file_length(file);	
  }

  lock_release(&syscall_lock);
  return -1;
}
int syscall_read (int fd, void *buffer, unsigned length)
{
  //printf("SYSCALL READ(%s) : start\n", thread_current()->name);
	catch_addr_error(buffer);
	catch_addr_error(buffer + length - 1);
  if (fd == 0)
  {
    //printf("SYSCALL READ(%s) : console\n", thread_current()->name);
  	uint8_t keyboard_input = input_getc();
  	memcpy(buffer, &keyboard_input, sizeof(uint8_t));
  	return sizeof(uint8_t);
  }

  struct file *file = find_file_by_fd(fd);
  //printf("read file = %p\n", file);

  if (file == NULL)
  {
    //printf("SYSCALL READ(%s) : fail\n", thread_current()->name);
  	return -1;
  }

  //printf("SYSCALL READ(%s) : let's do read\n", thread_current()->name);
  lock_acquire(&syscall_lock);
  int read_l = file_read(file, buffer, length);
  lock_release(&syscall_lock);
  //printf("readl = %d\n", read_l);
  //printf("SYSCALL READ(%s) : read finished\n", thread_current()->name);
  return read_l;
}
int syscall_write (int fd, const void *buffer, unsigned length)
{
	catch_addr_error(buffer);
	catch_addr_error(buffer + length - 1);
  int ret;
  struct file *file;

  //printf("write file start = %d %d\n", fd, length);
  lock_acquire(&syscall_lock);
  if (fd == 1)
  {
    putbuf(buffer, length);
    ret = length;
  }
  else
  {
		file = find_file_by_fd(fd);

		if (file == NULL)
	   	ret = -1;
	  else
    {
			ret = file_write(file, buffer, length);
    }

  }
  lock_release(&syscall_lock);

  //printf("write file done = %d %d, to %p\n", fd, ret, file);
  return ret;
}

void syscall_seek (int fd, unsigned position)
{
  lock_acquire(&syscall_lock);
  struct file *file = find_file_by_fd(fd);
  file_seek(file, position);
  lock_release(&syscall_lock);
}

unsigned syscall_tell (int fd)
{
  return file_tell(find_file_by_fd(fd));
}

void syscall_close (int fd)
{
  struct file_fd_name *ffn = find_mapping_by_fd(fd);

  if(ffn == NULL)
  	return;

  ffn->file_state = FILE_CLOSED;
}

bool check_syscall_lock()
{
  if (syscall_lock.semaphore.value == 0)
    return true;
  else
    return false;
}

void syscall_lock_acquire()
{
  thread_current()->sys_lock_hold = 1;
  lock_acquire(&syscall_lock);
}

void syscall_lock_release()
{
  thread_current()->sys_lock_hold = 0;
  lock_release(&syscall_lock);
}