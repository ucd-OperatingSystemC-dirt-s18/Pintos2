/*The 80x86 is a segmented architecture. The Global Descrptor Table (GDT) is a table that describes the segement in use. These files set up the GDT. You should not need to modify these files for any of the project. You can read the code if you're interested in house the GDT works. */

#ifndef USERPROG_GDT_H
#define USERPROG_GDT_H

#include "threads/loader.h"

/* Segment selectors.
   More selectors are defined by the loader in loader.h. */
#define SEL_UCSEG       0x1B    /* User code selector. */
#define SEL_UDSEG       0x23    /* User data selector. */
#define SEL_TSS         0x28    /* Task-state segment. */
#define SEL_CNT         6       /* Number of segments. */

void gdt_init (void);

#endif /* userprog/gdt.h */


