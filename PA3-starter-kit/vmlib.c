/*
 * vmlib.c -- helpers provided for Programming Assignment 3.
 * GIVEN FILE: you do not need to change this.
 *
 * This file contains the demand paging loop, the FIFO baseline, and all the
 * printing. The only thing it does not contain is the page replacement
 * decision, which is yours to write in vmsim.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vmlib.h"

#define SWEEP_MAX 7      /* the frame sweep tries 1 to 7 frames */
#define LINE 59

static void rule(char c)
{
    for (int i = 0; i < LINE; i++) putchar(c);
    putchar('\n');
}

/* ===================================================================== */
/*  Loading the reference string                                          */
/* ===================================================================== */

int load_refstring(VmSim *sim, const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "error: cannot open reference string '%s'\n", path);
        return -1;
    }

    sim->nrefs = 0;
    char tok[64];
    char line[1024];

    while (fgets(line, sizeof line, f) != NULL) {
        char *p = line;
        if (*p == '#') continue;                /* a comment line */
        while (*p != '\0') {
            while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) p++;
            if (*p == '\0' || *p == '#') break;
            int n = 0;
            while (*p != '\0' && !isspace((unsigned char)*p) && *p != ','
                   && n < (int)sizeof tok - 1)
                tok[n++] = *p++;
            tok[n] = '\0';

            if (sim->nrefs >= MAX_REFS) {
                fprintf(stderr, "error: more than %d references in '%s'\n",
                        MAX_REFS, path);
                fclose(f);
                return -1;
            }
            char *end = NULL;
            long page = strtol(tok, &end, 10);
            if (end == tok) {
                fprintf(stderr, "error: '%s' is not a page number\n", tok);
                fclose(f);
                return -1;
            }
            sim->ref[sim->nrefs]      = (int)page;
            sim->is_write[sim->nrefs] = (*end == 'w' || *end == 'W') ? 1 : 0;
            sim->nrefs++;
        }
    }
    fclose(f);

    if (sim->nrefs == 0) {
        fprintf(stderr, "error: '%s' contains no references\n", path);
        return -1;
    }

    /* remember just the file name, for the headings */
    const char *base = strrchr(path, '/');
    base = (base == NULL) ? path : base + 1;
    snprintf(sim->refname, sizeof sim->refname, "%s", base);
    return 0;
}

void vm_reset(VmSim *sim, int nframes)
{
    if (nframes < 1) nframes = 1;
    if (nframes > MAX_FRAMES) nframes = MAX_FRAMES;

    sim->nframes = nframes;
    for (int i = 0; i < MAX_FRAMES; i++) {
        sim->frame[i].page      = -1;
        sim->frame[i].load_time = 0;
        sim->frame[i].last_used = 0;
        sim->frame[i].uses      = 0;
        sim->frame[i].ref_bit   = 0;
        sim->frame[i].dirty_bit = 0;
        sim->frame[i].age       = 0;
    }
    sim->t          = 0;
    sim->faults     = 0;
    sim->hits       = 0;
    sim->evictions  = 0;
    sim->writebacks = 0;
    sim->hand       = 0;
    sim->recording  = 1;

    for (int t = 0; t < MAX_REFS; t++) {
        sim->fault_at[t]  = 0;
        sim->victim_at[t] = -1;
        for (int i = 0; i < MAX_FRAMES; i++) sim->hist[t][i] = -1;
    }
}

/* ===================================================================== */
/*  Small questions your algorithm may ask                                */
/* ===================================================================== */

int find_page(const VmSim *sim, int page)
{
    for (int i = 0; i < sim->nframes; i++)
        if (sim->frame[i].page == page) return i;
    return -1;
}

int find_free_frame(const VmSim *sim)
{
    for (int i = 0; i < sim->nframes; i++)
        if (sim->frame[i].page == -1) return i;
    return -1;
}

int next_use(const VmSim *sim, int page, int from)
{
    for (int t = from; t < sim->nrefs; t++)
        if (sim->ref[t] == page) return t;
    return sim->nrefs;          /* never used again */
}

/* ===================================================================== */
/*  The demand paging loop                                                */
/* ===================================================================== */

void run_simulation(VmSim *sim)
{
    for (sim->t = 0; sim->t < sim->nrefs; sim->t++) {
        int t     = sim->t;
        int page  = sim->ref[t];
        int write = sim->is_write[t];

        /* Aging shifts every register right once per reference, whether the
         * access hits or faults. Harmless for every other algorithm. */
        for (int i = 0; i < sim->nframes; i++)
            sim->frame[i].age = (unsigned char)(sim->frame[i].age >> 1);

        int f = find_page(sim, page);

        if (f >= 0) {
            /* ---- hit: the page is resident, no fault ---- */
            sim->hits++;
            sim->frame[f].last_used = t;
            sim->frame[f].uses++;
            sim->frame[f].ref_bit   = 1;
            sim->frame[f].age      |= 0x80;
            if (write) sim->frame[f].dirty_bit = 1;
            if (sim->verbose)
                printf("  t=%-3d page %-3d %s  HIT  in frame %d\n",
                       t, page, write ? "write" : "read ", f);
        } else {
            /* ---- page fault ---- */
            sim->faults++;
            sim->fault_at[t] = 1;

            f = find_free_frame(sim);
            if (f < 0) {
                /* no free frame: the replacement algorithm decides */
                f = choose_victim(sim);
                if (f < 0 || f >= sim->nframes) {
                    fprintf(stderr,
                            "error: choose_victim() returned %d at t=%d, "
                            "which is not a frame index in 0..%d\n",
                            f, t, sim->nframes - 1);
                    exit(1);
                }
                sim->evictions++;
                sim->victim_at[t] = f;
                if (sim->frame[f].dirty_bit) sim->writebacks++;
                if (sim->verbose)
                    printf("  t=%-3d page %-3d %s  FAULT, evict page %d "
                           "from frame %d%s\n",
                           t, page, write ? "write" : "read ",
                           sim->frame[f].page, f,
                           sim->frame[f].dirty_bit ? " (dirty, written back)" : "");
            } else if (sim->verbose) {
                printf("  t=%-3d page %-3d %s  FAULT, free frame %d\n",
                       t, page, write ? "write" : "read ", f);
            }

            /* bring the page in */
            sim->frame[f].page      = page;
            sim->frame[f].load_time = t;
            sim->frame[f].last_used = t;
            sim->frame[f].uses      = 1;
            sim->frame[f].ref_bit   = 1;
            sim->frame[f].dirty_bit = write;
            sim->frame[f].age       = 0x80;
        }

        /* record physical memory as it stands now */
        for (int i = 0; i < sim->nframes; i++)
            sim->hist[t][i] = sim->frame[i].page;
    }
    sim->t = sim->nrefs - 1;        /* leave t inside the string */
}

/* ===================================================================== */
/*  The FIFO baseline                                                     */
/* ===================================================================== */

/* FIFO, written out by hand so that it does not need choose_victim().
 * This is the baseline every topic is compared against. */
int fifo_faults(const VmSim *sim, int nframes)
{
    if (nframes < 1) nframes = 1;
    if (nframes > MAX_FRAMES) nframes = MAX_FRAMES;

    int page[MAX_FRAMES], load[MAX_FRAMES], faults = 0;
    for (int i = 0; i < nframes; i++) { page[i] = -1; load[i] = 0; }

    for (int t = 0; t < sim->nrefs; t++) {
        int want = sim->ref[t], found = 0;
        for (int i = 0; i < nframes; i++)
            if (page[i] == want) { found = 1; break; }
        if (found) continue;                      /* a hit */

        faults++;
        int f = -1;
        for (int i = 0; i < nframes; i++)
            if (page[i] == -1) { f = i; break; }  /* a free frame */
        if (f < 0) {
            f = 0;                                /* oldest arrival wins */
            for (int i = 1; i < nframes; i++)
                if (load[i] < load[f]) f = i;
        }
        page[f] = want;
        load[f] = t;
    }
    return faults;
}

/* Re-run the student's algorithm on a private copy. */
int my_faults(const VmSim *sim, int nframes)
{
    VmSim *copy = malloc(sizeof *copy);
    if (copy == NULL) {
        fprintf(stderr, "error: out of memory in my_faults()\n");
        exit(1);
    }
    *copy = *sim;
    vm_reset(copy, nframes);
    copy->verbose   = 0;
    copy->recording = 0;      /* so a topic's log is not appended to */
    run_simulation(copy);
    int n = copy->faults;
    free(copy);
    return n;
}

/* ===================================================================== */
/*  Output                                                                */
/* ===================================================================== */

void print_header(const VmSim *sim, const char *algorithm_name)
{
    rule('=');
    printf("  %s\n", algorithm_name);
    printf("  reference string : %s  (%d references)\n", sim->refname, sim->nrefs);
    printf("  page frames      : %d\n", sim->nframes);
    rule('=');
    printf("\n");
}

void print_frame_table(const VmSim *sim)
{
    /* Eight references per block, so it stays readable in a terminal. */
    const int per_line = 11;

    printf("Frame contents after every reference  ");
    printf("(. = frame empty, F = page fault)\n\n");

    for (int start = 0; start < sim->nrefs; start += per_line) {
        int end = start + per_line;
        if (end > sim->nrefs) end = sim->nrefs;

        printf("  t     :");
        for (int t = start; t < end; t++) printf(" %3d", t);
        printf("\n  ref   :");
        for (int t = start; t < end; t++)
            printf(" %2d%c", sim->ref[t], sim->is_write[t] ? 'w' : ' ');
        printf("\n         ");
        for (int t = start; t < end; t++) printf("----");
        printf("\n");

        for (int i = 0; i < sim->nframes; i++) {
            printf("  f%-5d:", i);
            for (int t = start; t < end; t++) {
                if (sim->hist[t][i] < 0) printf("   .");
                else                     printf(" %3d", sim->hist[t][i]);
            }
            printf("\n");
        }

        printf("         ");
        for (int t = start; t < end; t++) printf("----");
        printf("\n  fault :");
        for (int t = start; t < end; t++)
            printf("   %c", sim->fault_at[t] ? 'F' : '.');
        printf("\n\n");
    }
}

void print_frames_now(const VmSim *sim)
{
    printf("Physical memory as it stands at t=%d\n\n", sim->t);
    printf("  frame   page   loaded   last use   uses   ref   dirty   age\n");
    printf("  ---------------------------------------------------------------\n");
    for (int i = 0; i < sim->nframes; i++) {
        const Frame *fr = &sim->frame[i];
        if (fr->page < 0) {
            printf("  %5d   free\n", i);
            continue;
        }
        printf("  %5d   %4d   %6d   %8d   %4d   %3d   %5d   ",
               i, fr->page, fr->load_time, fr->last_used, fr->uses,
               fr->ref_bit, fr->dirty_bit);
        for (int b = 7; b >= 0; b--) putchar((fr->age >> b) & 1 ? '1' : '0');
        printf("\n");
    }
    printf("\n");
}

void print_summary(const VmSim *sim)
{
    double rate = (double)sim->faults / (double)sim->nrefs;
    rule('-');
    printf("  References        : %d\n", sim->nrefs);
    printf("  Page faults       : %d\n", sim->faults);
    printf("  Hits              : %d\n", sim->hits);
    printf("  Fault rate        : %.4f  (%.2f%%)\n", rate, rate * 100.0);
    printf("  Hit rate          : %.4f  (%.2f%%)\n", 1.0 - rate,
           (1.0 - rate) * 100.0);
    printf("  Evictions         : %d\n", sim->evictions);
    printf("  Dirty write-backs : %d   (evicted pages that had to be saved)\n",
           sim->writebacks);
    rule('-');
    printf("\n");
}

void print_eat(const VmSim *sim)
{
    /* Lecture slide 42. A memory access takes 100 ns; servicing a page fault
     * takes 8 ms, which is 8,000,000 ns. */
    const double ma    = 100.0;
    const double fault = 8000000.0;

    double p   = (double)sim->faults / (double)sim->nrefs;
    double eat = (1.0 - p) * ma + p * fault;

    rule('-');
    printf("  EFFECTIVE ACCESS TIME   (memory 100 ns, fault service 8 ms)\n");
    rule('-');
    printf("  p (fault rate)    = %d / %d = %.4f\n", sim->faults, sim->nrefs, p);
    printf("  EAT = (1 - p) x 100 ns + p x 8,000,000 ns\n");
    printf("      = %.4f x 100 + %.4f x 8000000\n", 1.0 - p, p);
    printf("      = %.1f ns\n", eat);
    printf("  Slowdown vs no paging at all : %.1f times\n", eat / ma);
    printf("\n  For EAT to stay under 110 ns, which is a 10%% slowdown,\n");
    printf("  the fault rate would have to be below %.8f, or about 1 in %.0f.\n",
           10.0 / (fault - ma), (fault - ma) / 10.0);
    rule('-');
    printf("\n");
}

void print_vs_fifo(const VmSim *sim, const char *algorithm_name)
{
    int base = fifo_faults(sim, sim->nframes);
    int mine = sim->faults;
    int diff = mine - base;
    double pct = (base == 0) ? 0.0 : 100.0 * (double)diff / (double)base;

    rule('=');
    printf("  %s  vs  FIFO BASELINE\n", algorithm_name);
    printf("  %s, %d frames\n", sim->refname, sim->nframes);
    rule('=');
    printf("  %-22s %10s %10s %10s %9s\n",
           "metric", "FIFO", "yours", "diff", "change");
    rule('-');
    printf("  %-22s %10d %10d %+10d %8.1f%%\n",
           "page faults", base, mine, diff, pct);
    printf("  %-22s %10.4f %10.4f %+10.4f %8s\n",
           "fault rate",
           (double)base / sim->nrefs, (double)mine / sim->nrefs,
           (double)diff / sim->nrefs, "");
    rule('-');
    if (diff < 0)
        printf("  Your algorithm took %d FEWER faults than FIFO here.\n", -diff);
    else if (diff > 0)
        printf("  Your algorithm took %d MORE faults than FIFO here.\n"
               "  That is a real result, not a bug. Explain why in your report.\n",
               diff);
    else
        printf("  Your algorithm tied with FIFO here. Explain why nothing in\n"
               "  this reference string lets the two algorithms differ.\n");
    rule('=');
    printf("\n");
}

void print_frame_sweep(const VmSim *sim, const char *algorithm_name)
{
    int fb[SWEEP_MAX + 1], mb[SWEEP_MAX + 1];
    for (int n = 1; n <= SWEEP_MAX; n++) {
        fb[n] = fifo_faults(sim, n);
        mb[n] = my_faults(sim, n);
    }

    rule('=');
    printf("  FRAME SWEEP: does more memory always help?\n");
    printf("  %s\n", sim->refname);
    rule('=');
    printf("  algorithm : %s\n", algorithm_name);
    rule('-');
    printf("  %-8s %10s %14s %12s\n", "frames", "FIFO", "yours", "difference");
    rule('-');
    for (int n = 1; n <= SWEEP_MAX; n++) {
        char fmark = (n > 1 && fb[n] > fb[n - 1]) ? '*' : ' ';
        char mmark = (n > 1 && mb[n] > mb[n - 1]) ? '*' : ' ';
        printf("  %-8d %9d%c %13d%c %+12d\n", n, fb[n], fmark, mb[n], mmark,
               mb[n] - fb[n]);
    }
    rule('-');

    int fa = 0, ma = 0;
    for (int n = 2; n <= SWEEP_MAX; n++) {
        if (fb[n] > fb[n - 1]) fa = 1;
        if (mb[n] > mb[n - 1]) ma = 1;
    }
    printf("  * marks a frame count where ADDING a frame made things worse.\n");
    printf("    That is Belady's anomaly (lecture slide 46).\n\n");
    printf("  FIFO on this string            : %s\n",
           fa ? "ANOMALOUS, more frames cost more faults"
              : "no anomaly on this string");
    printf("  Your algorithm on this string  : %s\n",
           ma ? "ANOMALOUS, more frames cost more faults"
              : "no anomaly on this string");
    rule('=');
    printf("\n");
}

int write_csv(const VmSim *sim, const char *algorithm_name, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "error: cannot write '%s'\n", path);
        return -1;
    }
    fprintf(f, "refstring,algorithm,frames,faults,fifo_faults,"
               "references,fault_rate,evictions,writebacks\n");
    for (int n = 1; n <= SWEEP_MAX; n++) {
        int mine = my_faults(sim, n);
        fprintf(f, "%s,%s,%d,%d,%d,%d,%.4f,,\n",
                sim->refname, algorithm_name, n, mine, fifo_faults(sim, n),
                sim->nrefs, (double)mine / sim->nrefs);
    }
    fprintf(f, "%s,%s,%d,%d,%d,%d,%.4f,%d,%d\n",
            sim->refname, algorithm_name, sim->nframes, sim->faults,
            fifo_faults(sim, sim->nframes), sim->nrefs,
            (double)sim->faults / sim->nrefs, sim->evictions, sim->writebacks);
    fclose(f);
    return 0;
}
