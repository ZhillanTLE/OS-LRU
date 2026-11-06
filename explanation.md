# Programming Assignment 3, Type 2: Least Recently Used (LRU)

**Group:** Zhillan Baniaksa 2506637174, Maglio Razzy E. 2596553616
**Class:** KKI

All figures below come from our own captured run in `sample_output.txt`.

---

## Part 1: Real memory, measured (`memlab`)

### Q1. Why VmRSS barely moves on allocation, then jumps after writing

The mechanism used is called as demand paging. malloc and mmap creates
the virtual address range which are the page table entries for it that will be marked invalid
and no physical frame has been assigned yet.

a frame is then allocated only if a page is touched, then will raise a page fault that
the OS services by the sequence on lecture slide 40 (trap, find a free frame, map, restart the instruction).


Our figures, for a 4,000,000-byte (3.81 MiB) request:

| step                     | malloc VmRSS        | mmap VmRSS          |
|--------------------------|---------------------|---------------------|
| before asking            | 1740 KiB            | 1740 KiB            |
| after asking             | **1744 KiB** (+4)   | **1740 KiB** (+0)   |
| after writing every int  | **5648 KiB** (+3908)| **5648 KiB** (+3908)|
| after free / munmap      | 1740 KiB            | 1740 KiB            |

Jump of 3908 KiB is 977 pages × 4 KiB: one page fault per page, and
exactly the number of pages the request rounds up to. The +4 KiB after
malloc is the one page malloc touched itself to write its chunk header.

### Q2. Bytes wasted in the last page

**1792 bytes.** 4,000,000 bytes needs 4,000,000 / 4096 = 976.56 pages,
rounded up to 977 pages = 4,001,792 bytes, and 4,001,792 − 4,000,000 = 1792
bytes of the last page are allocated but unused.

This is **internal fragmentation**. It can never be avoided because the page
is the smallest unit the MMU can map: a process cannot be given part of a
frame, so unless a request is an exact multiple of 4096 bytes, the last page
is partly empty.


The other kind is external fragmentation which is free memory
that exists in total but is split into holes *between* allocations, too small
or scattered to satisfy a contiguous request. Paging removes external
fragmentation entirely (any free frame can hold any page), at the price of
this small internal waste.

### Q3. Page offset of malloc vs mmap

| block  | first byte        | offset in page |
|--------|-------------------|----------------|
| malloc | `0x703c2f2a2010`  | **16**         |
| mmap   | `0x703c2f2a2000`  | **0**          |

`mmap` returns memory from the **kernel** and the **kernel** works only with whole pages so the block starts exactly on a page boundary (offset 0).

`malloc` is managed by the **C librarys allocator in user space**. The **C librarys allocator in user space** obtains pages from the **kernel** (for a request this large glibc itself uses `mmap` underneath) and then keeps its own 16‑byte bookkeeping header (the chunk size and flags `free()` needs) at the start. The pointer it returns is after that header, so offset 16. Both blocks even landed on the page `0x703c2f2a2`: the **malloc**ed region was freed and the **kernel** reused the same address for the `mmap`, and the only difference is **malloc**'s 16‑byte header.

### Q4. Static vs dynamic linking (`make sizes`)

| binary          | size     | `ldd` says                                   |
|-----------------|----------|----------------------------------------------|
| `memlab`        | **17K**  | needs `libc.so.6`, `ld-linux-x86-64.so.2`, `linux-vdso.so.1` |
| `memlab-static` | **937K** | `not a dynamic executable`                   |

The static binary is about 55 times bigger because the linker copied the parts of the C library that the program uses—like `printf` `malloc` `mmap` wrappers and the start-up code—directly into the executable. The `ldd` command shows no shared libraries for this binary because there is nothing left to resolve at run time. There is no loader request in the file at all. The dynamic binary on the other hand only includes our code plus some stubs. When the program starts the loader maps `libc.so.6` into memory.

- **Advantage of static:** it is self-contained. It runs on any kernel without needing specific library versions installed and it skips run-time symbol resolution.

- **Advantage of dynamic:** the files are smaller and one copy of `libc` in memory is shared by every process that uses it. A security fix, for the library applies to every program without needing to relink.

(Our run was on Ubuntu 24.04, x86-64, which is why the loader is
`ld-linux-x86-64.so.2`.)

---

## Part 2: The FIFO baseline

Before changing anything, the unmodified kit printed **15 page faults** on
`r1-lecture.txt` with 3 frames, matching the lecture's published FIFO answer. The FIFO column of our
comparison table in `sample_output.txt` shows the same 15.

---

## Part 3: LRU

### How the algorithm decides

LRU evicts the page that has gone unused for the longest time. The library
writes the current reference index into `last_used` on every reference, hit
or fault, so the victim is simply the frame with the **smallest `last_used`**:

```c
int choose_victim(VmSim *sim)
{
    /* LRU: evict the page whose last use is furthest in the past.
     * Strict '<' means ties go to the lower frame index. */
    int victim = 0;
    for (int i = 1; i < sim->nframes; i++)
        if (sim->frame[i].last_used < sim->frame[victim].last_used)
            victim = i;
    return victim;
}
```

The only difference, from the given FIFO is that load_time becomes last_used. FIFO asks when a page arrived. LRU asks when a page was last used. The less than means that a later frame only wins if frame is strictly older. Therefore ties go to the lower frame index. In practice there are no ties because one page is referenced at each time step so all last_used values are distinct. The function never writes any bookkeeping field.

### Our extra output: the LRU stack

`print_lru_stack()` prints, at every eviction, the resident pages ordered by
recency, most recent first, with each one's `last_used`. This is slide 49's
stack. It is rebuilt after the run from the recorded history (`hist`,
`victim_at`, `ref`), so it never touches the bookkeeping fields, and the
frame sweep's extra runs cannot pollute it.

The far-right page is always the one evicted, which shows the decision is
ordered by **recency, not arrival**. For example, from `r1-lecture.txt`:

```
  t     page in   stack, most recent first
  7     4         0(t=6) > 3(t=5) > 2(t=3)   evict 2
```

Page 0 is the oldest arrival in memory (t=1 before 3 at t=5 and 2 at t=3) so an arrival‑ordered rule would pick Page 0. LRU evicts 2 instead because Page 0 was used again at t=4 and t=6. FIFO really does make that mistake: it throws Page 0 out at t=5 and has to fault it straight back in at t=6. Page 0 is the lecture strings hottest page and keeping Page 0 is where LRUs win, on r1 comes from.

---

## Manual verification: `r1-lecture.txt`, 3 frames, first ten references

Rule: on a fault with memory full, evict the frame with the smallest
`last_used`.

| t | ref | result | why                                              | frames (f0 f1 f2) |
|---|-----|--------|--------------------------------------------------|-------------------|
| 0 | 7   | fault  | frame 0 free                                     | 7 . .             |
| 1 | 0   | fault  | frame 1 free                                     | 7 0 .             |
| 2 | 1   | fault  | frame 2 free                                     | 7 0 1             |
| 3 | 2   | fault  | full; last used 7@0, 0@1, 1@2 → evict **7** (f0) | 2 0 1             |
| 4 | 0   | hit    | 0 now last used at 4                             | 2 0 1             |
| 5 | 3   | fault  | 2@3, 0@4, 1@2 → evict **1** (f2)                 | 2 0 3             |
| 6 | 0   | hit    | 0 now last used at 6                             | 2 0 3             |
| 7 | 4   | fault  | 2@3, 0@6, 3@5 → evict **2** (f0)                 | 4 0 3             |
| 8 | 2   | fault  | 4@7, 0@6, 3@5 → evict **3** (f2)                 | 4 0 2             |
| 9 | 3   | fault  | 4@7, 0@6, 2@8 → evict **0** (f1)                 | 4 3 2             |

8 faults and 2 hits in the first ten references. This matches the program's
frame table column for column (t=9: f0=4, f1=3, f2=2) and the first five rows
of its LRU stack (evict 7, 1, 2, 3, 0). Over all 22 references the program
reports **12 faults**, the lecture's published LRU answer (slide 48).

---

## Comparison against FIFO

### Results (required runs)

| reference string | frames | FIFO | LRU | diff | change  |
|------------------|:------:|:----:|:---:|:----:|:-------:|
| r1-lecture.txt   | 3      | 15   | 12  | −3   | −20.0%  |
| r2-belady.txt    | 3      | 9    | 10  | +1   | +11.1%  |
| r3.txt           | 3      | 10   | 9   | −1   | −10.0%  |
| r4.txt           | 3      | 11   | 13  | +2   | +18.2%  |
| r5.txt           | 3      | 12   | 12  | 0    | 0.0%    |
| r6.txt           | 3      | 15   | 15  | 0    | 0.0%    |
| r7-reuse.txt     | 3      | 9    | 9   | 0    | 0.0%    |
| r7-reuse.txt     | 4      | 6    | 5   | −1   | −16.7%  |

LRU wins on 3 runs, loses on 2, and ties on 3.

### What LRU does better, and why

LRU keeps pages that are **still being used**, even if they arrived long ago.
FIFO cannot tell a hot page from a dead one, because all it knows is arrival
order.

The clearest case is **r1-lecture.txt (15 → 12)**. Page 0 is referenced 7
times out of 22 (`… 0 3 0 4 2 3 0 3 0 3 2 1 2 0 …`). FIFO loaded it at t=1,
so FIFO treats it as old: it evicts page 0 three times (t=5, 9, 19), and each
time page 0 is needed at the very next reference (t=6, 10, 20). LRU's stack
shows 0 near the top most of the time (`0(t=4) > …`, `0(t=6) > …`), so it
evicts page 0 only twice (t=9, 15) (see the t=7 row above).

**r7-reuse.txt at 4 frames (6 → 5)** shows the same thing at a longer reuse
distance. Page 1 is used at t=0 and t=4, then not again until t=14. Page 4 is
used once, at t=3. When page 5 arrives at t=13, FIFO evicts page 1 (the
oldest arrival) while LRU evicts page 4 (least recently used). Page 1 is
needed at t=14: a hit for LRU, a fault for FIFO.

### What LRU does worse

**r4.txt: 13 vs 11** (`4 2w 0 2 6 1w 0 5 4 1 0w 2 3 6`).

```
  t     page in   stack, most recent first
  4     6         2(t=3) > 0(t=2) > 4(t=0)   evict 4
  5     1         6(t=4) > 2(t=3) > 0(t=2)   evict 0
  6     0         1(t=5) > 6(t=4) > 2(t=3)   evict 2
```

The page LRU incorrectly protected is **page 2**. I saw that page 2 was accessed again at time t=3, which was a hit and that action moved page 2 above page 0 in the stack. So, at time t=5 LRU removed page 0 even though page 0 is needed at the next reference, time t=6 while LRU kept page 2 even though page 2 is not used again until time t=11. FIFO removes pages in arrival order; page 2 arrived at time t=1 before page 0 arrived at time t=2. Consequently FIFO removed page 2. Kept page 0 giving a hit at time t=6. From that point the two strategies diverge and LRU keeps evicting the page that will return soon. Recent use was a bad predictor here because the string does not repeat its recent past.

**r2-belady.txt: 10 vs 9** (`1 2 3 4 1 2 5 1 2 3 4 5`).

```
  9     3         2(t=8) > 1(t=7) > 5(t=6)   evict 5
  10    4         3(t=9) > 2(t=8) > 1(t=7)   evict 1
  11    5         4(t=10) > 3(t=9) > 2(t=8)  evict 2
```

Pages **1 and 2** were hits at t=7 and t=8, so LRU ranked them as hot and
evicted page 5 at t=9. But 1 and 2 are never used again, and 5 is needed at
t=11. FIFO kept 5 (it was the newest arrival) and got a hit there. LRU paid
for protecting pages whose burst of use had just ended.

### What LRU costs
The algorithm looks simple on paper. The cost comes from hardware.

`Last_used` must be written on every memory reference whether it is a hit or a fault, not just on faults. On r1 that is 22 timestamp updates, for 22 references while FIFO only records `load_time` on its 15 faults. In a machine every load and store would need that update. Slide 49 shows the two implementations. Neither is cheap.

- **Counter:** a clock value is copied into the page-table entry on every access, and eviction must search all entries for the smallest (O(n), as in our loop).
- **Stack:** a doubly linked list of pages; every reference moves its page to the top (up to 6 pointer updates per access), but the victim is always at the bottom, so no search is needed.

Both require hardware help on every memory access, which's why real systems use approximations (reference bit, second chance / clock) instead of exact LRU.

### Where the difference is largest, and why

The largest difference at 3 frames is r1-lecture.txt, 3 fewer faults (−20%). It is the string with the strongest temporal locality: a small hot set (0 3 2) that is reused within a short distance and one page (0) that arrived early but stays hot. That is the case where "arrived long ago" (FIFO) and "used long ago" (LRU) disagree and LRU is right. The largest loss is r4 (+2, +18.2%) the string that does not repeat its past.

The ties have a cause too:

- r6 is 100% faults for both (15/15). With 3 frames every page's reuse distance is at least 3 other distinct pages (2 5 3 1 2 4 5 3 2 1 4 5 2 3 6) so whichever page is evicted the next reference is never resident. Recency information cannot help when nothing is reused soon enough.

- r5 (12 vs 12) and r7 at 3 frames (9 vs 9) have the same problem for most of the string. In r7 the loop 1 2 3 4 1 2 3 cycles 4 pages through 3 frames so the least recently used page is always the next one needed, which is LRU's worst case. With 4 frames (r7 at -f 4) the loop fits, and LRU pulls ahead.

---

## Belady's anomaly and the frame sweep

Our sweep (faults for 1 to 7 frames):

| string     | FIFO 1–7                    | LRU 1–7                  |
|------------|-----------------------------|--------------------------|
| r1-lecture | 22 15 15 10 9 6 6           | 22 17 12 8 7 6 6         |
| r2-belady  | 12 12 9 **10** 5 5 5        | 12 12 10 8 5 5 5         |
| r3         | 13 11 10 7 6 6 6            | 13 11 9 6 6 6 6          |
| r4         | 14 13 11 11 10 7 7          | 14 13 13 10 10 8 7       |
| r5         | 13 13 12 10 10 9 7          | 13 13 12 9 9 7 7         |
| r6         | 15 15 15 9 6 6 6            | 15 15 15 12 6 6 6        |
| r7-reuse   | 15 9 9 6 5 5 5              | 15 9 9 5 5 5 5           |

- **FIFO shows the anomaly on r2-belady.txt:** 9 faults with 3 frames, but
  **10 with 4**. Adding a frame made it worse (slide 46).
- **LRU never increases** on any of the seven strings, at any frame count.

The two claims are different in kind:

- For FIFO on r1, r3–r7, the anomaly **did not happen on these strings**.
- For LRU, the anomaly **cannot happen on any string**. This is a
  guarantee. LRU is a **stack algorithm**: with n frames
  the resident set is always the n most recently used pages, which is always
  a subset of the n+1 most recently used pages. So any reference that hits
  with n frames also hits with n+1, and faults can only stay the same or go
  down. Our sweep is consistent with that, but the sweep is not the proof; the
  inclusion property is.

The sweep also shows LRU is **not uniformly better** at every size: r1 at 2
frames (17 vs 15), r6 at 4 frames (12 vs 9), r4 at 6 frames (8 vs 7). Being
anomaly-free means monotonic, not optimal.

---

## Effective access time

From our run of `r1-lecture.txt` (memory 100 ns, fault service 8 ms):

```
p   = 12 / 22 = 0.5455
EAT = (1 − p) × 100 ns + p × 8,000,000 ns
    = 0.4545 × 100 + 0.5455 × 8,000,000
    = 4,363,681.8 ns      (about 4.36 ms, 43,636.8 times slower than no paging)
```

EAT on the other strings: r2 6,666,683.3 ns, r3 5,538,492.3 ns, r4
7,428,578.6 ns, r5 7,384,623.1 ns, r6 8,000,000.0 ns (every reference a
fault), r7 4,800,040.0 ns at 3 frames and 2,666,733.3 ns at 4 frames.

The fault-service term dominates completely. A disk fault is 80,000 times
slower than a memory access so even a tiny fault rate swamps the 100 ns.
To keep EAT under **110 ns** (a 10% slowdown) the program says the fault
rate must be below **0.00000125, 1 fault in 800,000 references**. Our
strings are 10–20 references on 3 frames, mostly cold-start (compulsory)
faults, so their fault rates of 33–100% are 5 orders of magnitude away from
that. A real system only reaches 1 in 800,000 because a process's working set
fits in its frames and the vast majority of accesses are hits. This is why the
choice of replacement algorithm and having enough frames matters so much.

---

## Who did what

- **Zhillan Baniaksa:** Created helper function to print LRU's stack and explanation.md
- **Maglio Razzy E.:** Implemented the LRU algorithm and ran the outputs    
