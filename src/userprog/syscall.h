/*Whenever a user process wants to access some kernel functionality, it invokes a system call. This is a skeleton system call handler. Currently, it just prints a message and terminates the user process. Add code to do everything elese needed by system calls.*/

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "user/syscall.h"
#include "../lib/stdint.h"


struct user_syscall {
  // because each argument is passed through interrupt handler in 32 bit format
  int syscall_index;
  int args[3];
  int arg_count;
};



void syscall_init (void);
void halt (void);
void exit (int status);
int exec (const char *file);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
/*
 * Not used in Project 2
mapid_t mmap (int fd, void *addr);
void munmap (mapid_t mapid);
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char name[READDIR_MAX_LEN + 1]);
bool isdir (int fd);
int inumber (int fd);
 */
#endif /* userprog/syscall.h */
