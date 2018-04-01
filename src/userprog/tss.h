/*The task-state segment (tss) is used for 8-x86 architectural task switching. Pintos uses the TSS only for switching stacks when a user process enters an interrupt handler, as does Linux. You shoud not need to modify these files for any of the projects. */

#ifndef USERPROG_TSS_H
#define USERPROG_TSS_H

#include <stdint.h>

struct tss;
void tss_init (void);
struct tss *tss_get (void);
void tss_update (void);

#endif /* userprog/tss.h */
