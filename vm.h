/*
 * vm.h -- the data structures for Programming Assignment 3.
 * GIVEN FILE: read it, you do not need to change it.
 *
 * One Frame is one physical page frame. The array of frames inside VmSim is
 * the whole of physical memory as far as this simulation is concerned, and
 * the fields on each frame are the bookkeeping bits that the various page
 * replacement algorithms need. A real page table entry carries the same
 * information: a valid bit, a reference bit and a modify (dirty) bit.
 */

#ifndef VM_H
#define VM_H

#define MAX_FRAMES 16    /* most frames the simulation will ever be asked for */
#define MAX_REFS  512    /* longest reference string that can be loaded */

/* One physical page frame. */
typedef struct {
    int page;        /* page number resident here, or -1 when the frame is free */
    int load_time;   /* reference index at which this page was brought in (FIFO) */
    int last_used;   /* reference index of its most recent use (LRU)             */
    int uses;        /* how many times it has been referenced (LFU, MFU)         */
    int ref_bit;     /* reference bit: set on every access (Second Chance)       */
    int dirty_bit;   /* modify bit: set on a write (Enhanced Second Chance)      */
    unsigned char age;  /* 8-bit shift register, newest use in the high bit (Aging) */
} Frame;

/* The whole state of one simulation run. */
typedef struct VmSim {
    /* physical memory */
    Frame frame[MAX_FRAMES];
    int   nframes;              /* how many frames are actually in use */

    /* the workload */
    int   ref[MAX_REFS];        /* the reference string: page numbers */
    int   is_write[MAX_REFS];   /* 1 if that reference is a write, 0 if a read */
    int   nrefs;
    char  refname[64];          /* the file it came from, for the headings */

    /* where the run has got to */
    int   t;                    /* index of the reference being handled right now */

    /* results */
    int   faults;               /* page faults */
    int   hits;                 /* references that were already resident */
    int   evictions;            /* faults that had to throw a page out */
    int   writebacks;           /* evictions of a dirty page: a disk write */

    /* algorithm scratch space */
    int   hand;                 /* the clock hand, for Second Chance */

    /* recorded history, so the frame-by-frame table can be printed at the end */
    int   hist[MAX_REFS][MAX_FRAMES];  /* what was in each frame after reference t */
    int   fault_at[MAX_REFS];          /* 1 if reference t caused a page fault    */
    int   victim_at[MAX_REFS];         /* frame evicted at reference t, else -1   */

    int   verbose;              /* -v: print what happens at every reference */

    /* 1 during the real run, 0 while the library is re-running your
     * algorithm on a private copy to build the frame sweep. If your extra
     * output collects a log as it goes, guard the collecting with this so
     * the sweep's extra runs do not append to it. */
    int   recording;
} VmSim;

#endif /* VM_H */
