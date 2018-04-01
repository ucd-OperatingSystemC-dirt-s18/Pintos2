#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "process.h"
#include "../threads/thread.h"

static thread_func start_process NO_RETURN;
static bool load(user_program *p_user_prog, void (**eip)(void), void **esp);

extern struct list all_list;


//A2 - Implemented argument parsing Calling strtok_r does not interact wiht the orginal cmd line string. */
tid_t process_execute(const char *file_name)//Implement support passing arguments 
{
//Remember: Accesses files, but those test pass for free.
    char *fn_copy = NULL;
    tid_t tid;
    char *save_ptr;
    char *fn_name = NULL;

    fn_copy = palloc_get_page(0);//Make a copy of FILE_NAME.
    if (fn_copy == NULL)//Otherwise there's a race between the caller and load().
        return TID_ERROR;
    strlcpy(fn_copy, file_name, PGSIZE);

    fn_name = (char *) malloc(strlen(file_name) + 1);//Allocate mem
    strlcpy(fn_name, file_name, strlen(file_name) + 1);//create copy of file_name

    strtok_r(fn_name, " ", &save_ptr);//Extract fn_name from cmd line string

    tid = thread_create(fn_name, PRI_DEFAULT, start_process, fn_copy);//Create a new thread to execute FILE_NAME.

    if (tid == TID_ERROR)
        palloc_free_page(fn_copy);

    sema_down(&thread_current()->child_sema);
    if (!thread_current()->success)
        return -1;

    return tid;
}
//_______________________________________________________________

static void start_process(void *file_name_) {//everything leads here!!!

    char *throwaway = NULL;
    char *arg = NULL;
    char *cpy = NULL;
    char *save_ptr = NULL;
    char *file_name = file_name_;
    int arg_index = 0;

    user_program user_prog;//user_program struct stores file_name and arguments
    user_program *pup = &user_prog;

    cpy = (char *) malloc(strlen(file_name_) + 1);
    strlcpy(cpy, file_name_, strlen(file_name_) + 1);
    strtok_r(file_name, " ", &throwaway);//extracts file name and arguments using strtok_r in order and placed in consecutive spots 
    user_prog.file_name = file_name;//first get file name
    arg = strtok_r(cpy, " ", &save_ptr);
    arg = strtok_r(NULL, " ", &save_ptr);
    
while (arg != NULL && arg_index < 24) {//then up to 16 arguments separated by spaces
        user_prog.args[arg_index] = arg;
        arg = strtok_r(NULL, " ", &save_ptr);
        arg_index++;
    }

    user_prog.arg_count = arg_index + 1;

    bool success;
    struct intr_frame if_;

    memset(&if_, 0, sizeof if_);//Initialize interrupt frame and load executable.
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
    success = load(pup, &if_.eip, &if_.esp);
    free(cpy);
    
    palloc_free_page(file_name);// If load failed, quit.
    if (!success) {
        thread_current()->parent->success = false;//If the process does not start successfully, set bool success of thread to false
        sema_up(&thread_current()->parent->child_sema);
        thread_exit();
    } else {//In both cases must sema UP the child_sema lock of the parent thread
        thread_current()->parent->success = true;
        sema_up(&thread_current()->parent->child_sema);
    }

    /* Start the user process by simulating a return from an
       interrupt, implemented by intr_exit (in
       threads/intr-stubs.S).  Because intr_exit takes all of its
       arguments on the stack in the form of a `struct intr_frame',
       we just point the stack pointer (%esp) to our stack frame
       and jump to it. */
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
    NOT_REACHED();
}
//__________________________________________________________________________

int process_wait(tid_t child_tid) {
 
    struct list_elem *e;
    struct child_parent *_c = NULL;
    struct list_elem *_e = NULL;

    for (e = list_begin(&thread_current()->child_processes); e != list_end(&thread_current()->child_processes); e = list_next(e)) {
        struct child_parent *c = list_entry(e, struct child_parent, elem);
        if (c->tid == child_tid) {//If TID is invalid or if it was not a child of the calling process
            _c = c;
            _e = e;
        }
    }

    if (!_c || !_e)
        return -1;//returns -1 immediately, without waiting.

    thread_current()->waiting_on_thread = _c->tid;//or if process_wait() has already been successfully called for the given TID

    if (!_c->has_exited)
        sema_down(&thread_current()->child_sema);

    int status = _c->exit_code;
    list_remove(_e);

    return status;
}
//_____________________________________________________________________

void process_exit(void) {
    
    struct thread *cur = thread_current();
    uint32_t *pd;

    if (cur->exit_code == -100)//if freed
        exit(-1);

    int exit_code = cur->exit_code;

    printf("%s: exit(%d)\n",cur->name,exit_code);
		
    file_lock_acquire();
    file_close(thread_current()->cur_file);
    close_all(&thread_current()->files);
    file_lock_release();//free

    /* Destroy the current process's page directory and switch back
       to the kernel-only page directory. */
    pd = cur->pagedir;
    if (pd != NULL) {
        /* Correct ordering here is crucial.  We must set
           cur->pagedir to NULL before switching page directories,
           so that a timer interrupt can't switch back to the
           process page directory.  We must activate the base page
           directory before destroying the process's page
           directory, or our active page directory will be one
           that's been freed (and cleared). */
        cur->pagedir = NULL;
        pagedir_activate(NULL);
        pagedir_destroy(pd);
    }
}//_______________________________________________________________

void process_activate(void) {
    struct thread *t = thread_current();//Set up cpu current thread

    pagedir_activate(t->pagedir);//Activate thread's page tables.

    /* Set thread's kernel stack for use in processing
       interrupts. */
    tss_update();
}
//_________________________________________________________________

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in //printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr {
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack(user_program *p_user_prog, void **esp);

static bool validate_segment(const struct Elf32_Phdr *, struct file *);

static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool load(user_program *p_user_prog, void (**eip)(void), void **esp) {
    struct thread *t = thread_current();
    struct Elf32_Ehdr ehdr;
    struct file *file = NULL;
    off_t file_ofs;
    bool success = false;
    int i;

    file_lock_acquire();
    /* Allocate and activate page directory. */
    t->pagedir = pagedir_create();
    if (t->pagedir == NULL)
        goto done;
    process_activate();

    file = filesys_open((*p_user_prog).file_name);


    if (file == NULL) {
        //printf("load: %s: open failed\n", (*p_user_prog).file_name);
        goto done;
    }

    /* Read and verify executable header. */
    if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr
        || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7)
        || ehdr.e_type != 2
        || ehdr.e_machine != 3
        || ehdr.e_version != 1
        || ehdr.e_phentsize != sizeof(struct Elf32_Phdr)
        || ehdr.e_phnum > 1024) {
        //printf("load: %s: error loading executable\n", (*p_user_prog).file_name);
        goto done;
    }

    /* Read program headers. */
    file_ofs = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) {
        struct Elf32_Phdr phdr;

        if (file_ofs < 0 || file_ofs > file_length(file))
            goto done;
        file_seek(file, file_ofs);

        if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
            goto done;
        file_ofs += sizeof phdr;
        switch (phdr.p_type) {
            case PT_NULL:
            case PT_NOTE:
            case PT_PHDR:
            case PT_STACK:
            default:
                /* Ignore this segment. */
                break;
            case PT_DYNAMIC:
            case PT_INTERP:
            case PT_SHLIB:
                goto done;
            case PT_LOAD:
                if (validate_segment(&phdr, file)) {
                    bool writable = (phdr.p_flags & PF_W) != 0;
                    uint32_t file_page = phdr.p_offset & ~PGMASK;
                    uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
                    uint32_t page_offset = phdr.p_vaddr & PGMASK;
                    uint32_t read_bytes, zero_bytes;
                    if (phdr.p_filesz > 0) {
                        /* Normal segment.
                           Read initial part from disk and zero the rest. */
                        read_bytes = page_offset + phdr.p_filesz;
                        zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE)
                                      - read_bytes);
                    } else {
                        /* Entirely zero.
                           Don't read anything from disk. */
                        read_bytes = 0;
                        zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
                    }
                    if (!load_segment(file, file_page, (void *) mem_page,
                                      read_bytes, zero_bytes, writable))
                        goto done;
                } else
                    goto done;
                break;
        }
    }

    if (!setup_stack(p_user_prog, esp))//Set up stack
        goto done;

    *eip = (void (*)(void)) ehdr.e_entry;//Start address
    success = true;

    file_deny_write(file);
    thread_current()->cur_file = file;

    done:
		file_lock_release();//load successful or not 
    return success;
}
//__________________________________________________________

static bool install_page(void *upage, void *kpage, bool writable);

static bool validate_segment(const struct Elf32_Phdr *phdr, struct file *file) {
    /* p_offset and p_vaddr must have the same page offset. */
    if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
        return false;

    /* p_offset must point within FILE. */
    if (phdr->p_offset > (Elf32_Off) file_length(file))
        return false;

    /* p_memsz must be at least as big as p_filesz. */
    if (phdr->p_memsz < phdr->p_filesz)
        return false;

    /* The segment must not be empty. */
    if (phdr->p_memsz == 0)
        return false;

    /* The virtual memory region must both start and end within the
       user address space range. */
    if (!is_user_vaddr((void *) phdr->p_vaddr))
        return false;
    if (!is_user_vaddr((void *) (phdr->p_vaddr + phdr->p_memsz)))
        return false;

    /* The region cannot "wrap around" across the kernel virtual
       address space. */
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
        return false;

    /* Disallow mapping page 0.
       Not only is it a bad idea to map page 0, but if we allowed
       it then user code that passed a null pointer to system calls
       could quite likely panic the kernel by way of null pointer
       assertions in memcpy(), etc. */
    if (phdr->p_vaddr < PGSIZE)
        return false;

    return true;
}
//_________________________________________________________________

static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
             uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    file_seek(file, ofs);
    while (read_bytes > 0 || zero_bytes > 0) {//Must be writable w/ 0 bytes @ upage
        /* Calculate how to fill this page.
           We will read PAGE_READ_BYTES bytes from FILE
           and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* Get a page of memory. */
        uint8_t *kpage = palloc_get_page(PAL_USER);
        if (kpage == NULL)
            return false;//false if a memory allocation error or disk read error occurs

        /* Load this page. */
        if (file_read(file, kpage, page_read_bytes) != (int) page_read_bytes) {
            palloc_free_page(kpage);
            return false;//false if a memory allocation error or disk read error occurs
        }
        memset(kpage + page_read_bytes, 0, page_zero_bytes);

        /* Add the page to the process's address space. */
        if (!install_page(upage, kpage, writable)) {
            palloc_free_page(kpage);
            return false;//false if a memory allocation error or disk read error occurs
        }

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
    }
    return true;//Return true if successful
}
//______________________________________________________________

static bool setup_stack(user_program *user_prog, void **esp) {
    uint8_t *kpage;
    bool success = false;

    kpage = palloc_get_page(PAL_USER | PAL_ZERO);//minimal stack by mapping a zeroed page at the top of user virtual memory
    if (kpage != NULL) {
        success = install_page(((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
        if (success)
            *esp = PHYS_BASE;
        else {
            palloc_free_page(kpage);
            return success;
        }

        int num_bytes = 0;
        char *throwaway = NULL;
        char *arg = NULL;
        void *save_ptr = *esp;

        for (int i = (*user_prog).arg_count - 2; i >= 0; --i) {// Extracts file name and arguments using strtok_r; first get file name,
            size_t len = strlen((*user_prog).args[i]) + 1; // then up to 24 arguments separated by spaces
            *esp -= len;
            strlcpy(*esp, (*user_prog).args[i], len);
           
            num_bytes += (int) len;
        }

        *esp -= strlen((*user_prog).file_name) + 1;
        strlcpy(*esp, (*user_prog).file_name, strlen((*user_prog).file_name) + 1);
 
        num_bytes += strlen((*user_prog).file_name) + 1;
        int k = 0;
        for (int j = (num_bytes + k) % 4; j != 0; ++k, j = (num_bytes + k) % 4) {
       
            *esp -= sizeof(NULL_BYTE);//check word align
            memset(*esp, NULL_BYTE, sizeof(NULL_BYTE));
        }

        //printf("Push null\n");
        *esp -= sizeof(char *);
        memset(*esp, 0, sizeof(char *));

        //printf("Push string pointers\n");
        for (int i = (*user_prog).arg_count - 2; i >= 0; --i) {
            *esp -= sizeof(char *);
            save_ptr -= strlen((*user_prog).args[i]) + 1;
            memcpy(*esp, &save_ptr, sizeof(char *));
            //printf("Pointer to argv[%d]: 0x%x : 0x%x \n", i + 1, (unsigned int) *esp, (unsigned int) save_ptr);
        }

        *esp -= sizeof(char *);
        save_ptr -= strlen((*user_prog).file_name) + 1;
        memcpy(*esp, &save_ptr, sizeof(char *));//argvpoint argument

        save_ptr = *esp;
        *esp -= sizeof(char **);
        memcpy(*esp, &save_ptr, sizeof(char **));

        *esp -= sizeof(unsigned int);
        memcpy(*esp, &(*user_prog).arg_count, sizeof(int));
        *esp -= sizeof(void (*)());
        memset(*esp, 0, sizeof(void (*)()));

    }
    return success;
}
//____________________________________________________________________

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool install_page(void *upage, void *kpage, bool writable) {
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
       address, then map our page there. */
    return (pagedir_get_page(t->pagedir, upage) == NULL
            && pagedir_set_page(t->pagedir, upage, kpage, writable));
}
//___________________________________________________________________

int process_affix_file(struct file *f) {
    ASSERT (f != NULL);
    struct process_file *aFile = malloc(sizeof(struct process_file));
    if (aFile == NULL) {
        return -1;
    }
    aFile->file_ptr = f;
    aFile->fid = thread_current()->fid_count++;
    list_push_back(&thread_current()->files, &aFile->elem);
    return aFile->fid;
}
//___________________________________________________________________

struct file *process_get_file(int fid) {
    struct thread *curr_thread = thread_current();
    for (struct list_elem *e = list_begin(&curr_thread->files); e != list_end(&curr_thread->files); e = list_next(e)) {
        struct process_file *procFile = list_entry(e, struct process_file, elem);
        if (procFile->fid == fid) {
            return procFile->file_ptr;
        }
    }
    return NULL;
}
//___________________________________________________________________
