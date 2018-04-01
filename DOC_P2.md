		     +--------------------------+
       	       	     |		CS 140		|
		     | PROJECT 2: USER PROGRAMS	|
		     | 	   DESIGN DOCUMENT     	|
		     +--------------------------+

---- GROUP ----

Christina Tsui christina.tsui@ucdenver.edu

Dalton Burke dalton.burke@ucdenver.edu

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.
Passing 80/80 tests on the VM provided by the professor in the assignment details.

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.
Also used the information provided by the proffesor on the course Slack channel.

			   ARGUMENT PASSING
			   ================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
    typedef struct user_program - holds file_name, argc, and argv.

---- ALGORITHMS ----

>> A2: Briefly describe how you implemented argument parsing.  How do
>> you arrange for the elements of argv[] to be in the right order?
>> How do you avoid overflowing the stack page?
    There is a token made of the file_name which is used to call thread_create.
    This starts the thread start_process(). The argc and argc are used after the
    command like is parsed and saved in the struct user_program. Keeping the
    arguments in order, the stack is then initialized. Once a return is simulated,
    the interrupt forces the tread to start.

---- RATIONALE ----

>> A3: Why does Pintos implement strtok_r() but not strtok()?
    To achieve thread saftey using Re-entry. This allows the code in such a way that it
    can be partially exectued by one task, reentered by another task and then resumed
    from the orginal task.

>> A4: In Pintos, the kernel separates commands into a executable name
>> and arguments.  In Unix-like systems, the shell does this
>> separation.  Identify at least two advantages of the Unix approach.
    a)The kernal has certian jobs. Separating these commands allows for the kernal to
    not be bogged down with tasks that can be done elsewhere.
    b)This also helps to organize the load and make sure that any errors could be caught
    before entering the kernal mode.

			     SYSTEM CALLS
			     ============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

    struct child_parent - Indicates child status
    struct user_syscall - Parsed arguments and syscall_index held here.
    struct process_file - File descriptor
    struct lock file_lock - File lock (global)

    struct file* cur_file - Pointer to current file in execution
    struct list files - Current open files
    struct thread *parent - Parent thread
    struct semaphore child_sema - Semaphore child lock
    struct list child_processes - Child pricesses
    bool success - Success stracker
    int fid_count - File descriptor count
    int waiting_on_thread - tid waiting on
    int exit_code - Exit

>> B2: Describe how file descriptors are associated with open files.
>> Are file descriptors unique within the entire OS or just within a
>> single process?
    a)File descriptors assist when opening a file, the operating system creates an
    entry to represent that file and store the information about that opened file.
    b) Unique just within a sinlge process.


---- ALGORITHMS ----

>> B3: Describe your code for reading and writing user data from the
>> kernel.

>> B4: Suppose a system call causes a full page (4,096 bytes) of data
>> to be copied from user space into the kernel.  What is the least
>> and the greatest possible number of inspections of the page table
>> (e.g. calls to pagedir_get_page()) that might result?  What about
>> for a system call that only copies 2 bytes of data?  Is there room
>> for improvement in these numbers, and how much?
    The least number of times called: 1
    The greatest number of times called : 4096
    Using pagedir_get_page() for pointer validation will give you the same as the
    least number of times called, max would be 8192.



>> B5: Briefly describe your implementation of the "wait" system call
>> and how it interacts with process termination.

>> B6: Any access to user program memory at a user-specified address
>> can fail due to a bad pointer value.  Such accesses must cause the
>> process to be terminated.  System calls are fraught with such
>> accesses, e.g. a "write" system call requires reading the system
>> call number from the user stack, then each of the call's three
>> arguments, then an arbitrary amount of user memory, and any of
>> these can fail at any point.  This poses a design and
>> error-handling problem: how do you best avoid obscuring the primary
>> function of code in a morass of error-handling?  Furthermore, when
>> an error is detected, how do you ensure that all temporarily
>> allocated resources (locks, buffers, etc.) are freed?  In a few
>> paragraphs, describe the strategy or strategies you adopted for
>> managing these issues.  Give an example.

---- SYNCHRONIZATION ----

>> B7: The "exec" system call returns -1 if loading the new executable
>> fails, so it cannot return before the new executable has completed
>> loading.  How does your code ensure this?  How is the load
>> success/failure status passed back to the thread that calls "exec"?

>> B8: Consider parent process P with child process C.  How do you
>> ensure proper synchronization and avoid race conditions when P
>> calls wait(C) before C exits?  After C exits?  How do you ensure
>> that all resources are freed in each case?  How about when P
>> terminates without waiting, before C exits?  After C exits?  Are
>> there any special cases?

---- RATIONALE ----

>> B9: Why did you choose to implement access to user memory from the
>> kernel in the way that you did?
    It was suggested in the design document to be implemented in such a way.

>> B10: What advantages or disadvantages can you see to your design
>> for file descriptors?
Advantage:
    None of the temporary files get left out.
    Can maintain the file name
    Easier to implement

>> B11: The default tid_t to pid_t mapping is the identity mapping.
>> If you changed it, what advantages are there to your approach?
    Did not change.
