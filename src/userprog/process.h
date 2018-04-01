/*Loads ELF binaries and starts processes*/

/* You must ensure that Pintos does not terminate untill the intial process exits. The supplied Pintos code tried to do this by calling process_wait() (in 'userprog/process.c') from main() (in 'threads/init.c'). We suggest that you implemented process_wait() according to the comment at the top of the function and then implement the wait system call in terms of process_wait(). */

#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#include "threads/thread.h"
#define NULL_BYTE ((char) 0) 

struct process_file {
    struct file* file_ptr;
    int fid;
    struct list_elem elem;
};

typedef struct {
    const char *file_name;
    char *args[24];
    int arg_count;
    void *parent;
    void *child;

    enum child_status {
        BEGIN,
        BUSY,
        FIN
    };
} user_program;

tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

int process_affix_file(struct file *f);
struct file *process_get_file(int fid);

#endif /* userprog/process.h */
