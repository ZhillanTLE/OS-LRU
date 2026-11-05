/*
 * vmlib.h -- helpers provided for Programming Assignment 3.
 * GIVEN FILE: you do not need to change this.
 *
 * Everything here is written for you. The only function you write lives in
 * vmsim.c and is called choose_victim().
 */

#ifndef VMLIB_H
#define VMLIB_H

#include "vm.h"

/* --- the function YOU write, in vmsim.c -------------------------------- */

/* Choose which frame to throw out. Called by the library only when a page
 * fault has happened AND every frame is already full, so you never have to
 * check for a free frame yourself. Return a frame index in 0 .. nframes-1.
 *
 * The page being brought in is sim->ref[sim->t]. */
int choose_victim(VmSim *sim);

/* Your topic's own extra output. Called once after the run. */
void print_topic_extra(VmSim *sim);

/* The name of your algorithm, defined at the top of vmsim.c. */
extern const char *ALGORITHM_NAME;

/* --- setting up -------------------------------------------------------- */

/* Read a reference string from a file into sim. Returns 0 on success, -1 on
 * failure. One reference per whitespace-separated token: a page number,
 * optionally followed by 'w' to mark it a write. Lines starting with '#' and
 * blank lines are ignored.  Example:  7 0 1w 2 0 3w */
int  load_refstring(VmSim *sim, const char *path);

/* Empty every frame and zero every counter, keeping the reference string.
 * Called for you before each run, including each run of the frame sweep. */
void vm_reset(VmSim *sim, int nframes);

/* --- running the simulation -------------------------------------------- */

/* Walk the whole reference string, calling choose_victim() on every fault
 * that needs an eviction. This is the demand paging loop from the lecture. */
void run_simulation(VmSim *sim);

/* --- things your algorithm may want to ask ----------------------------- */

/* Which frame holds this page? Returns the frame index, or -1 if the page is
 * not resident. */
int  find_page(const VmSim *sim, int page);

/* The first free frame, or -1 if memory is full. */
int  find_free_frame(const VmSim *sim);

/* When is `page` referenced again, counting from reference index `from`?
 * Returns the index of the next reference to it, or sim->nrefs if it is never
 * referenced again. Useful for exactly one algorithm. */
int  next_use(const VmSim *sim, int page, int from);

/* --- output ------------------------------------------------------------ */

void print_header(const VmSim *sim, const char *algorithm_name);

/* The lecture's frame-by-frame table: one column per reference, one row per
 * frame, and an F on the bottom row wherever a page fault happened. */
void print_frame_table(const VmSim *sim);

/* Faults, hits, fault rate, evictions and writebacks. */
void print_summary(const VmSim *sim);

/* Effective access time, from the fault rate. Lecture slide 42. */
void print_eat(const VmSim *sim);

/* The current contents of physical memory, with every bookkeeping bit.
 * Handy while you are debugging your algorithm. */
void print_frames_now(const VmSim *sim);

/* --- the FIFO baseline ------------------------------------------------- */

/* How many page faults FIFO WOULD take on this reference string with this
 * many frames. Pure function: it does not disturb the run in progress. This
 * is the baseline every topic compares against, and it is given to you so
 * that you only ever implement one algorithm. */
int  fifo_faults(const VmSim *sim, int nframes);

/* How many faults YOUR algorithm takes with this many frames. Re-runs the
 * simulation on a private copy, so it is safe to call at any time. */
int  my_faults(const VmSim *sim, int nframes);

/* Print the standard "your algorithm vs FIFO" table for the current frame
 * count, with the difference and the percentage change. Required for every
 * topic. */
void print_vs_fifo(const VmSim *sim, const char *algorithm_name);

/* Print faults for 1 to 7 frames, for FIFO and for your algorithm, and say
 * for each whether adding a frame ever made things WORSE. That is Belady's
 * anomaly, lecture slide 46. Required for every topic. */
void print_frame_sweep(const VmSim *sim, const char *algorithm_name);

/* Append one row per frame count to a CSV file. Returns 0 on success. */
int  write_csv(const VmSim *sim, const char *algorithm_name, const char *path);

#endif /* VMLIB_H */
