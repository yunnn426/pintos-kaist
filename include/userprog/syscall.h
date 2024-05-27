#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <include/lib/user/syscall.h>
#include <stdbool.h>

void syscall_init (void);

/* system call functions */
void halt();
void exit(int status);

/* file manipulation */
int exec (const char *cmd_line);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
void check_address(void *addr);
int open (const char *file);
int filesize (int fd);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* process hierarchy */
pid_t fork (const char *thread_name);

#endif /* userprog/syscall.h */