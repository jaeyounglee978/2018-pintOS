#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


bool check_syscall_lock();
void syscall_lock_acquire();
void syscall_lock_release();
void syscall_init (void);
#endif /* userprog/syscall.h */
