/*
 * memlab.c -- Programming Assignment 3, Part 1.
 * GIVEN FILE, COMPLETE. You do not write any of this.
 *
 * Your job for Part 1 is to compile it, run it, put the output in your
 * report, and answer the four questions in your ASSIGNMENT.md about what you
 * saw. Read the code first: every number it prints is explained by a comment.
 *
 * What it demonstrates, all of it from the W06 and W07 lecture:
 *
 *   1. Logical addresses, split into a page number and an offset  (slide 10)
 *   2. Demand paging: memory you have been GIVEN is not memory that
 *      physically EXISTS yet. Nothing is resident until you touch it (slide 37)
 *   3. Internal fragmentation, because mappings are rounded up to whole
 *      pages                                                      (slide 9)
 *   4. malloc against mmap, two ways to ask the kernel for memory
 *   5. Static against dynamic linking, which is the Makefile's job (slide 31)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define N_INTS (1000 * 1000)       /* 1,000,000 ints, about 3.8 MiB */
#define SHOW   10                  /* how many values to print back */

/* ---------------------------------------------------------------------- *
 *  Reading our own resident set size out of /proc
 * ---------------------------------------------------------------------- */

/*
 * VmRSS in /proc/self/status is the RESIDENT SET SIZE: how much of our
 * address space is actually in physical memory right now, in kibibytes.
 *
 * This is the number to watch. VmSize would tell us how much address space we
 * have reserved, which is a much larger and much less interesting number.
 */
static long vmrss_kib(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    if (f == NULL) return -1;

    char line[256];
    long kib = -1;
    while (fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &kib);
            break;
        }
    }
    fclose(f);
    return kib;
}

static void report(const char *stage, long kib, long baseline)
{
    printf("    %-34s VmRSS = %7ld KiB   (%+ld KiB)\n",
           stage, kib, kib - baseline);
}

/* ---------------------------------------------------------------------- *
 *  Splitting a real address the way the MMU does
 * ---------------------------------------------------------------------- */

/*
 * The hardware splits every logical address into a page number and an offset
 * within that page, exactly as on lecture slide 10. With a 4096-byte page,
 * 4096 = 2^12, so the low 12 bits are the offset and everything above them is
 * the page number.
 *
 * The page numbers printed here are VIRTUAL page numbers. An ordinary process
 * is not allowed to see which physical frame it was given: that mapping
 * belongs to the kernel, which is the protection the whole scheme exists to
 * provide.
 */
static void decompose(const char *label, const void *addr, long pagesize)
{
    unsigned long a    = (unsigned long)addr;
    int           bits = 0;
    for (long p = pagesize; p > 1; p >>= 1) bits++;

    unsigned long page   = a >> bits;
    unsigned long offset = a & (unsigned long)(pagesize - 1);

    printf("    %-22s %#18lx   page %#14lx   offset %5lu\n",
           label, a, page, offset);
}

/* ---------------------------------------------------------------------- *
 *  One experiment, run twice: once on malloc, once on mmap
 * ---------------------------------------------------------------------- */

/* `base` must be measured BEFORE the allocation, in main, or the "+0 KiB"
 * below would be true by construction instead of being a measurement. */
static void experiment(const char *how, int *a, size_t bytes, long pagesize,
                       long base)
{
    printf("\n  %s\n", how);
    printf("  ------------------------------------------------------------\n");
    printf("    requested                          %zu bytes (%.2f MiB)\n",
           bytes, (double)bytes / (1024.0 * 1024.0));
    printf("    VmRSS before we asked              %7ld KiB\n", base);

    decompose("first byte", a, pagesize);
    decompose("last byte",  (char *)a + bytes - 1, pagesize);

    /* Internal fragmentation: the kernel can only hand out whole pages, so a
     * request that is not an exact multiple of the page size is rounded up,
     * and the slack at the end of the last page is allocated to us but can
     * never be used. Lecture slide 9. */
    size_t pages  = (bytes + (size_t)pagesize - 1) / (size_t)pagesize;
    size_t rounded = pages * (size_t)pagesize;
    printf("    rounded up to                      %zu pages = %zu bytes\n",
           pages, rounded);
    printf("    internal fragmentation             %zu bytes wasted in the"
           " last page\n", rounded - bytes);

    report("after asking for the memory", vmrss_kib(), base);
    printf("      ^ barely moved. The address space exists, but almost no\n");
    printf("        physical frame has been allocated yet. That is DEMAND\n");
    printf("        PAGING: the pages are marked invalid in the page table\n");
    printf("        and will be faulted in one at a time when touched.\n");

    /* Now touch every element, writing them in REVERSE order so that the
     * first page we touch is the last page of the mapping. Each untouched
     * page costs one page fault, and the kernel allocates a frame for it. */
    for (size_t i = N_INTS; i-- > 0; )
        a[i] = (int)i;

    report("after writing every element", vmrss_kib(), base);
    printf("      ^ THERE is the physical memory. One page fault per page,\n");
    printf("        about %zu of them, each one handled by the sequence on\n",
           pages);
    printf("        lecture slide 40.\n");

    /* Read some of it back, so that the compiler cannot decide the writes
     * above were pointless and delete the loop. */
    printf("    first %d values                     ", SHOW);
    for (int i = 0; i < SHOW; i++) printf("%d ", a[i]);
    printf("\n");
}

/* ---------------------------------------------------------------------- */

int main(void)
{
    long   pagesize = sysconf(_SC_PAGESIZE);
    size_t bytes    = (size_t)N_INTS * sizeof(int);

    printf("============================================================\n");
    printf("  memlab: real memory, measured\n");
    printf("============================================================\n");
    printf("    page size                          %ld bytes\n", pagesize);
    printf("    sizeof(int)                        %zu bytes\n", sizeof(int));
    printf("    VmRSS at start                     %ld KiB\n", vmrss_kib());

    /* ---- experiment 1: the C library's allocator ---- */
    long base = vmrss_kib();            /* measured BEFORE the allocation */
    int *heap = malloc(bytes);
    if (heap == NULL) {
        fprintf(stderr, "malloc of %zu bytes failed\n", bytes);
        return 1;
    }
    experiment("malloc / free", heap, bytes, pagesize, base);

    long before_free = vmrss_kib();
    free(heap);
    printf("    after free()                       VmRSS = %7ld KiB   "
           "(%+ld KiB)\n", vmrss_kib(), vmrss_kib() - before_free);

    /* ---- experiment 2: asking the kernel directly ---- */
    /*
     * MAP_ANONYMOUS means the mapping is not backed by any file, so it is
     * just memory. MAP_PRIVATE means our writes are ours alone and are not
     * shared with any other process: that is the copy-on-write behaviour on
     * lecture slide 38.
     */
    base = vmrss_kib();                 /* measured BEFORE the mapping */
    int *raw = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (raw == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    experiment("mmap / munmap  (MAP_ANONYMOUS | MAP_PRIVATE)",
               raw, bytes, pagesize, base);

    before_free = vmrss_kib();
    if (munmap(raw, bytes) != 0) {
        perror("munmap");
        return 1;
    }
    printf("    after munmap()                     VmRSS = %7ld KiB   "
           "(%+ld KiB)\n", vmrss_kib(), vmrss_kib() - before_free);

    printf("\n============================================================\n");
    printf("  Now compare the two binaries:   make sizes\n");
    printf("  One is statically linked and one is dynamically linked.\n");
    printf("  Lecture slide 31 explains the difference.\n");
    printf("============================================================\n");
    return 0;
}
