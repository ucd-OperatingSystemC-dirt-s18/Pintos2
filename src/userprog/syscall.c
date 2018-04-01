/*Implrement the system all handler in 'userprog/syscall.c". The skeleton implrementation we provide "handles" system calls by terminating the prrocess.*/ 

#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "lib/string.h"
#include "lib/kernel/list.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "lib/stdbool.h"
#include "../threads/thread.h"


static void syscall_handler(struct intr_frame *);
static void user_memory_ok(const void *, int);
static int file_sys_ok();
static uint32_t get_user_int32(const void *);
static char * get_syscall_args(const void *, struct user_syscall *);
struct process_file *search_files(struct list *, int);


void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

struct process_file *search_files(struct list *files, int fid) {
    struct list_elem *e;
    
for (e = list_begin(files); e != list_end(files); e = list_next(e));
    {
        struct process_file *f = list_entry(e, struct process_file, elem);

        if (f->fid == fid) {
            return f;
        }
    }
    return NULL;
}
//___________________________________________________________________

static void user_memory_ok(const void *stack_pointer, int byte_size) {
  char * temp_stack_pointer = (char*)stack_pointer;
	for (int i = 0; i < byte_size; i++) {
    if (!is_user_vaddr(stack_pointer) || stack_pointer == NULL || pagedir_get_page(thread_current()->pagedir, temp_stack_pointer) == NULL) { exit(-1); }
    temp_stack_pointer++;
  }
}
//___________________________________________________________________

static char * get_syscall_args(const void *stack_pointer, struct user_syscall *new_syscall) {
    char * temp_stack_pointer = (char*)stack_pointer;
    new_syscall->arg_count = 0;

    for (int i = 0; i < 3; i++) {
      user_memory_ok(temp_stack_pointer, 4);

      new_syscall->args[i] = get_user_int32(temp_stack_pointer);
      new_syscall->arg_count++;
      temp_stack_pointer += 4;
    }
  return temp_stack_pointer;
}
//___________________________________________________________________

static uint32_t get_user_int32(const void *stack_pointer){
  char * temp_stack_pointer = (char*)stack_pointer;
  uint8_t byte_array[4];

  user_memory_ok(stack_pointer, 4);

  for (int i = 0; i < 4; i++) {
      
	byte_array[i] = get_user(temp_stack_pointer);//Get user
	temp_stack_pointer++;//increase the pointer for the stack
  }
    return *(uint32_t *) byte_array;
}
//___________________________________________________________________

static void syscall_handler(struct intr_frame *f) {

  char * stack_pointer = (char *) f->esp;
  struct user_syscall new_syscall;

  new_syscall.syscall_index = get_user_int32(stack_pointer);
  stack_pointer += 4;

  stack_pointer = get_syscall_args(stack_pointer, &new_syscall);
  int sys_call_num = (int) new_syscall.syscall_index;

  switch (sys_call_num) {
    case SYS_HALT :
      halt();
      break;  /* Halt the operating system. */

    case SYS_EXIT :
      exit((int) new_syscall.args[0]);
      break;/* Terminate this process. */

    case SYS_EXEC :
      f->eax = exec((const char *) new_syscall.args[0]);
      break; /* Start another process. */

    case SYS_WAIT :
      f-> eax = process_wait((tid_t) new_syscall.args[0]);
      break;  /* Wait for a child process to die. */

    case SYS_CREATE :
      f->eax = create((const char *) new_syscall.args[0], (unsigned) new_syscall.args[1]);
      break;  /* Create a file. */

    case SYS_REMOVE :
      f->eax = remove((const char *) new_syscall.args[0]);
      break;/* Delete a file. */

    case SYS_OPEN :
      f->eax = open((const char *) new_syscall.args[0]);
      break;/* Open a file. */

    case SYS_FILESIZE :
      f->eax = filesize((int) new_syscall.args[0]);
      break;/* Obtain a file's size. */

    case SYS_READ :
      f->eax = read((int) new_syscall.args[0], (const void *) new_syscall.args[1], (unsigned) new_syscall.args[2]);
      break;/* Read from a file. */

    case SYS_WRITE :
      f->eax = write((int) new_syscall.args[0], (const void *) new_syscall.args[1], (unsigned) new_syscall.args[2]);

      break; /* Write to a file. */

    case SYS_SEEK :
      seek((int) new_syscall.args[0], (unsigned) new_syscall.args[1]);
      break;/* Change position in a file. */

    case SYS_TELL :
      f->eax = tell((int) new_syscall.args[0]);

      break;/* Report current position in a file. */

    case SYS_CLOSE :
      close((int) new_syscall.args[0]);
      break;/* Close a file. */

    default:
        break;
  }
}
//___________________________________________________________________

void halt(void) { 
//Terminates Pintos by calling shutdown_power_off(), seldom used 
shutdown_power_off();}

void exit(int status) {
//Terminates the current user programs, returning status to the kernal. 
struct list_elem *e;

  for (e = list_begin(&thread_current()->parent->child_processes);
       e != list_end(&thread_current()->parent->child_processes);
       e = list_next(e)) {
    
struct child_parent *c = list_entry(e, struct child_parent, elem);
    if (c->tid == thread_current()->tid) {
     
	 c->has_exited = true;
	c->exit_code = status;
    }
  }

  thread_current()->exit_code = status;
// If the process's parent wait for it, this is the status that will be returned.
  if (thread_current()->parent->waiting_on_thread == thread_current()->tid)
    sema_up(&thread_current()->parent->child_sema);
  thread_exit();
}
//_____________________________________________________________________

pid_t exec(const char *file) {
    user_memory_ok(file,sizeof(file));//Runs the executable whos name is given in cms_line
    file_lock_acquire();
    char *f_cpy = malloc (strlen(file)+1);
    strlcpy (f_cpy, file, (strlen(file)+1));

    char * save_ptr;
    f_cpy = strtok_r (f_cpy, " ", &save_ptr);//passing any given argument and returns new process program id

    struct file* f = filesys_open (f_cpy);

    if(f == NULL)
    {
      file_lock_release();
      return -1;//Must return -1 pid
   
    }
    else
    {
      file_close(f);//if not -1 then it's not valid
      file_lock_release();
      
	return process_execute(file);//return from exec
    }
  }
//__________________________________________________________________

int wait(pid_t pid) {
    return process_wait(pid);//waits for a child process pid and retrieves the chil'd exit status
}
//__________________________________________________________________

bool create(const char *file, unsigned initial_size) {
    user_memory_ok(file,1);
    file_lock_acquire();

    bool error = filesys_create(file, initial_size);//create new file called initial_size

    file_lock_release();//return true if success
    return error;
}
//_________________________________________________________________

bool remove(const char *file) {
    file_lock_acquire();
    
bool error = filesys_remove(file);//Deletes file called file, returns true if success

    file_lock_release();
    return error;
}
//__________________________________________________________________

int open(const char *file) {
    user_memory_ok(file, 1);
    file_lock_acquire();
    struct file *oFile = filesys_open(file);//open file called file 
   
 if (oFile == NULL) 
	{
        file_lock_release();
        return -1;//returns a nonnegative int fd or -1 
	} 
else {
        int fd = process_affix_file(oFile);
        file_lock_release();
        return fd;
	}
}
//________________________________________________________________

int filesize(int fid) {
    file_lock_acquire();
    struct file *fs = process_get_file(fid);
    int f_size = -1;
    if (fs != NULL) {
        f_size = file_length(fs);//returns the size in bytes of the file open as fd
    }
    file_lock_release();
    return f_size;
}
//________________________________________________________________

void close(int fid) {
//closes file descriptor fd. ignore them for now. 
    struct list_elem *e;
    struct process_file *ff;
    file_lock_acquire();

    struct thread *curr_thread = thread_current();

    for (e = list_begin(&curr_thread->files); e != list_end(&curr_thread->files); e = list_next(e)) {
        ff = list_entry(e, struct process_file, elem);
        if (ff->fid == fid) {
            file_close(ff->file_ptr);
            list_remove(&ff->elem);
            curr_thread->fid_count--;
            break;
        }
    }
    
file_lock_release();
}
//____________________________________________________________

void close_all (struct list* files)
{
	struct list_elem *e;

	while (!list_empty (files))
	{
		e = list_pop_front (files);

		struct process_file *f = list_entry (e, struct process_file, elem);

		file_close(f->file_ptr);
		list_remove(e);
		free(f);
	}
}
//_________________________________________________________

int read(int fd, void *buffer, unsigned size) {
  user_memory_ok(buffer, 1);
  if (fd == 0) {
        char* buf = (char *) buffer;
        for (unsigned i = 0; i < size; ++i) {
            buf[i] = input_getc();
        }
        return size;
    }

    struct file *f = NULL;
    file_lock_acquire();

    f = process_get_file(fd);
    if (f == NULL) // If file could not be read, return -1
    {
        file_lock_release();
        return -1;
    }

    size = file_read(f, buffer, size);
    file_lock_release();
    return size;
}
//_______________________________________________________


int write(int fd, const void *buffer, unsigned size) {
    user_memory_ok(buffer, 1);
    struct file *f;
    file_lock_acquire();

    if (fd == 1)
    {
        putbuf(buffer, size);// Buffer writen using putbuf
        file_lock_release();
        return size;
    }

    f = process_get_file(fd);
    if (f == NULL)// If nothing written to file, return 0
    {
        file_lock_release();
        return 0;
    }

    size = file_write(f, buffer, size);
    file_lock_release();
    return size;
}
//_____________________________________________________

void seek(int fd, unsigned position) {
    struct file *f;
    f = process_get_file(fd);
    //f = search_files(f, fd);
    if (f == NULL)
    {
        printf("Seek failed. Exiting\n");
        exit(-1);// Should exit
    }
    //f->pos = position;
    file_seek(f, position);// Set file position to new position from file.c
    return;
}
//___________________________________________________

unsigned tell(int fd) {
    struct file *f;
    f= process_get_file(fd);
    if (f == NULL)
    {
        printf("Tell failed. Exiting\n");
        exit(-1);// Should exit
    }
    return file_tell(f);// Return file position from file.c
}
//______________________________________________________
