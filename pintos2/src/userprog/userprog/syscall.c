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
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

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
	int fd;
	char *name;
	struct file *file;
	enum FILE_STATE file_state;
	struct list_elem elem;
};

static struct list fd_name_mapping;
int fd_index = 2;

struct file *Find_file_by_fd(int fd);
struct file_fd_name *Find_mapping_by_name(const char *f);
struct file_fd_name *Find_mapping_by_fd(int fd);


struct file *
Find_file_by_fd(int fd)
{
	struct list_elem *e;
	e = list_begin(&fd_name_mapping);

	for (; e != list_end(&fd_name_mapping); e = list_next(e))
	{
		struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
		if (ffn->fd == fd)
			return ffn->file;
	}

	return NULL;
}

struct file_fd_name *
Find_mapping_by_name(const char *f)
{
	struct list_elem *e;
	e = list_begin(&fd_name_mapping);

	for (; e != list_end(&fd_name_mapping); e = list_next(e))
	{
		struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
		if (strcmp(ffn->name, f) == 0)
			return ffn;
	}

	return NULL;
}

struct file_fd_name *
Find_mapping_by_fd(int fd)
{
	struct list_elem *e;
	e = list_begin(&fd_name_mapping);

	for (; e != list_end(&fd_name_mapping); e = list_next(e))
	{
		struct file_fd_name *ffn = list_entry(e, struct file_fd_name, elem);
		if (ffn->fd == fd)
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
  int syscall_number;
  memcpy(&syscall_number, f->esp, sizeof(int));
  //printf("current esp = %p\n", f->esp);
  //printf("why syscall number = %d, %d\n", syscall_number, *(int *) f->esp);
  //printf("  SYSCALL %d\n", syscall_number);

  switch(syscall_number)
  {
  	case SYS_HALT:
  	{
  	  syscall_halt();
  	  break;
  	}
  	case SYS_EXIT:
  	{
  	  int status = 0;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(&status, f->esp + 4, sizeof(int));
  	  f->eax = status;
  	  syscall_exit(status);
  	  break;
  	}
  	case SYS_EXEC:
  	{
  	  void *file;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(file, f->esp + 4, sizeof(void *));
  	  pid_t pid = syscall_exec((const char *)file);
  	  f->eax = pid;
  	  break;
  	}
  	case SYS_WAIT:
  	{
  	  pid_t pid;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(&pid, f->esp + 4, sizeof(pid_t));
  	  int i = syscall_wait(pid);
  	  f->eax = i;
  	  break;
  	}
  	case SYS_CREATE:
  	{
  	  void *file;
  	  unsigned initial_size;
  	  catch_addr_error(f->esp + 8);
  	  memcpy(file, f->esp + 4, sizeof(void *));
  	  memcpy(&initial_size, f->esp + 8, sizeof(unsigned));
  	  bool b = syscall_create((const char *)file, initial_size);
  	  f->eax = b;
  	  break;
  	}
  	case SYS_REMOVE:
  	{
  	  void *file;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(file, f->esp + 4, sizeof(void *));
  	  bool b = syscall_remove((const char *)file);
  	  f->eax = b;

  	  break;
  	}
  	case SYS_OPEN:
  	{
  	  void *file;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(file, f->esp + 4, sizeof(void *));
  	  int i = syscall_open((const char *)file);
  	  f->eax = i;

  	  break;
  	}
  	case SYS_FILESIZE:
  	{
  	  int fd;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(&fd, f->esp + 4, sizeof(int));
  	  int i = syscall_filesize(fd);
  	  f->eax = i;

  	  break;
  	}
  	case SYS_READ:
  	{
  		catch_addr_error(f->esp + 12);
  	  int fd;
  	  unsigned length;
  	  uint32_t buffer_addr = *(void **)(f->esp + 8);
  	  memcpy(&fd, f->esp + 4, sizeof(int));
  	  memcpy(&length, f->esp + 12, sizeof(unsigned));

  	  int i = syscall_read(fd, (void *)buffer_addr, length);
  	  f->eax = i;
  	  //printf("sex\n");

  	  break;
  	}
  	case SYS_WRITE:
  	{
  		catch_addr_error(f->esp + 12);
  	  int fd;
  	  unsigned length;
  	  uint32_t buffer_addr = *(void **)(f->esp + 8);
  	  memcpy(&fd, f->esp + 4, sizeof(int));
  	  memcpy(&length, f->esp + 12, sizeof(unsigned));

  	  int i = syscall_write(fd, (const void *)buffer_addr, length);
  	  f->eax = i;

  	  break;
  	}
  	case SYS_SEEK:
  	{
  		catch_addr_error(f->esp + 8);
  	  int fd;
  	  unsigned position;
  	  memcpy(&fd, f->esp + 4, sizeof(int));
  	  memcpy(&position, f->esp + 8, sizeof(unsigned));

  	  syscall_seek(fd, position);

  	  break;
  	}
  	case SYS_TELL:
  	{
  	  int fd;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(&fd, f->esp + 4, sizeof(int));

  	  unsigned u = syscall_tell(fd);
  	  f->eax = u;

  	  break;
  	}
  	case SYS_CLOSE:
  	{
  	  int fd;
  	  catch_addr_error(f->esp + 4);
  	  memcpy(&fd, f->esp + 4, sizeof(int));
  	  syscall_close(fd);
  	  break;
  	}
  	default:
  	  break;
  }
  //thread_exit ();
}



void syscall_halt (void)
{
  shutdown_power_off();
}

void syscall_exit (int status)
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  //process_exit();
  thread_exit();
}

pid_t syscall_exec (const char *file)
{
  pid_t pid = process_execute(file);
  return pid;
}

int syscall_wait (pid_t pid)
{
  int ret = process_wait(pid);
  return ret;
}

bool syscall_create (const char *file, unsigned initial_size)
{
  lock_acquire(&syscall_lock);
  bool create = filesys_create(file, initial_size);
  lock_release(&syscall_lock);

  if (!create)
  	return create;

  struct file_fd_name *ffd;
  ffd = (struct file_fd_name *)malloc(sizeof(struct file_fd_name));

  memcpy(ffd->name, file, strlen(file));
  ffd->fd = fd_index;
  ffd->file_state = FILE_IDLE;
  fd_index++;

  list_push_back(&fd_name_mapping, &ffd->elem);

  return create;
}

bool syscall_remove (const char *file)
{
  lock_acquire(&syscall_lock);
  bool delete = filesys_remove(file);
  lock_release(&syscall_lock);

  if (delete)
  {
    struct file_fd_name *ffn = Find_mapping_by_name(file);
    list_remove(&ffn->elem);
    free(ffn);
    return delete;
  }
  else
  	return delete;
}

int syscall_open (const char *file)
{
  struct file *_file = filesys_open(file);

  if(_file == NULL)
  	return -1;

  struct file_fd_name *ffn = Find_mapping_by_name((const char *)_file);
  ffn->file = _file;
  ffn->file_state = FILE_OPEN;

  return ffn->fd;
}
int syscall_filesize (int fd)
{
  lock_acquire(&syscall_lock);
  struct file *file = Find_file_by_fd(fd);

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
	catch_addr_error(buffer);
	catch_addr_error(buffer + length - 1);
  if (fd == 0)
  {
  	uint8_t keyboard_input = input_getc();
  	memcpy(buffer, &keyboard_input, sizeof(uint8_t));
  	return sizeof(uint8_t);
  }

  struct file *file = Find_file_by_fd(fd);

  if (file == NULL)
  	return -1;

  int read_l = file_read(file, buffer, length);

  return read_l;
}
int syscall_write (int fd, const void *buffer, unsigned length)
{
	catch_addr_error(buffer);
	catch_addr_error(buffer + length - 1);
  int ret;

  lock_acquire(&syscall_lock);
  if (fd == 1)
  {
    putbuf(buffer, length);
    ret = length;
  }
  else
  {
	struct file *file = Find_file_by_fd(fd);

	if (file == NULL)
   	  ret = -1;

	int read_l = file_write(file, buffer, length);
	ret = read_l;
  }
  lock_release(&syscall_lock);

  return ret;
}

void syscall_seek (int fd, unsigned position)
{
  struct file *file = Find_file_by_fd(fd);
  file_seek(file, position);
}

unsigned syscall_tell (int fd)
{
  return file_tell(Find_file_by_fd(fd));
}

void syscall_close (int fd)
{
  struct file_fd_name *ffn = Find_mapping_by_fd(fd);
  ffn->file_state = FILE_CLOSED;
}
