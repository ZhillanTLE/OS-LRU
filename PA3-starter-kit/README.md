# Starter Kit: Programming Assignment 3

This is given to you. Copy the whole folder, then edit **`vmsim.c`** and
nothing else. There is exactly one function to write.

## Build and run

```bash
make                                 # builds vmsim, memlab, memlab-static
./memlab                             # Part 1: watch real memory being paged in
make sizes                           # Part 1: static against dynamic linking
./vmsim refs/r1-lecture.txt          # Part 2 and 3: 3 frames by default
./vmsim refs/r1-lecture.txt -f 4     # 4 page frames
./vmsim refs/r1-lecture.txt -v       # trace every single reference
make test                            # fault counts for all six strings
make clean
```

## The three parts

| Part | What you do | How much work |
|---|---|---|
| 1 | Run `memlab`, run `make sizes`, answer four questions about what you saw | no code |
| 2 | Run the **given** FIFO, check it against the lecture's published answer | no code |
| 3 | Write `choose_victim()` for **your** algorithm, and compare it to FIFO | one function |

**Part 1 is given complete.** `memlab.c` is a finished, commented program.
You do not write it and you do not modify it. You compile it, run it, put the
output in your report, and explain what it shows.

**Part 2 is given complete.** Out of the box `vmsim` runs FIFO, which is in
`vmsim.c` as `fifo_victim()`. FIFO is also the **baseline every topic is
compared against**, so you never have to implement a second algorithm: it is
already in the library as `fifo_faults()`.

**Part 3 is the assignment.** Replace the body of `choose_victim()`.

## What is in the kit

| File | Yours? | What it is |
|---|---|---|
| `vmsim.c` | **edit this** | `fifo_victim()` (given, the worked example) and `choose_victim()` (yours) |
| `memlab.c` | given, complete | Part 1. Read it, run it, do not change it |
| `vm.h` | read it | The `Frame` and `VmSim` structures |
| `vmlib.c` / `vmlib.h` | read it | The paging loop, the FIFO baseline, all the printing |
| `Makefile` | given | Build rules, including `make sizes` |
| `refs/r1..r7.txt` | given | The seven required reference strings |

## The one function you write

```c
int choose_victim(VmSim *sim)
{
    /* return the index of the frame to throw out */
}
```

The library calls it **only** when a page fault has happened **and** every
frame is already full, so you never have to check for a free frame. The page
being brought in is `sim->ref[sim->t]`.

Here is the whole of FIFO, which is what you are given:

```c
static int fifo_victim(VmSim *sim)
{
    int victim = 0;
    for (int i = 1; i < sim->nframes; i++)
        if (sim->frame[i].load_time < sim->frame[victim].load_time)
            victim = i;
    return victim;
}
```

Most of the seven topics are that shape with a different field compared. The
bookkeeping is kept up to date for you on every single reference, hit or
fault:

| Field | Meaning | The algorithm that wants it |
|---|---|---|
| `page` | the page resident in this frame | all |
| `load_time` | reference index when it was brought in | FIFO |
| `last_used` | reference index of its most recent use | LRU |
| `uses` | how many times it has been referenced | LFU, MFU |
| `ref_bit` | set to 1 on every access | Second Chance |
| `dirty_bit` | set to 1 on a write | Enhanced Second Chance |
| `age` | 8-bit register, newest use in the top bit | Aging |
| `sim->hand` | the clock hand; yours to move | Second Chance |

Plus two helpers: `next_use(sim, page, from)` says when a page is referenced
again, and `find_page(sim, page)` says which frame holds a page.

## Your self-check

`refs/r1-lecture.txt` is the reference string used throughout the lecture
(slides 45 to 48), so the answers are published:

```
3 frames:   FIFO 15 faults    Optimal 9 faults    LRU 12 faults
```

Run the kit before you change anything. It must say **15 page faults**. If it
does, your build is good and you have a known-correct reference point.

The given FIFO is also compared against the FIFO baseline, which gives
`+0` on every metric. That is not a mistake: it is the kit checking itself.
Once you write your own algorithm those numbers will differ.

## Ties

**When two frames are equally good victims, choose the LOWER FRAME INDEX**,
unless your topic says otherwise. Every number in the answer key is worked out
that way. Break ties differently and your output will not match, and you will
not be able to reproduce your own results.

## The seven reference strings

| File | Where it comes from | Why it is here |
|---|---|---|
| `r1-lecture.txt` | lecture slides 45 to 48 | The published answer. Your self-check |
| `r2-belady.txt` | lecture slide 46 | The string where FIFO gets WORSE with more frames |
| `r3.txt` | lecture exercise, slide 53, string 1 | |
| `r4.txt` | lecture exercise, slide 53, string 2 | |
| `r5.txt` | lecture exercise, slide 53, string 3 | |
| `r6.txt` | lecture exercise, slide 53, string 4 | |
| `r7-reuse.txt` | written for this assignment | Long reuse distances. **Run this one with `-f 4`** |

`r7-reuse.txt` leaves two pages untouched for more than eight references while
two others are hammered. It is there to separate the algorithms that keep a
full history from the ones that keep only a short window, and the difference
only appears at 4 frames, so run it with `-f 4`.

A `w` after a page number marks that reference a **write**, which sets the
modify (dirty) bit. Evicting a dirty page costs an extra disk write, which the
summary counts for you (slide 43). `r1` and `r2` are all reads so that they
match the lecture exactly; `r3` to `r6` contain writes.

## Reading the frame table

The simulator prints the lecture's own figure: one column per reference, one
row per frame, and an `F` wherever a page fault happened.

```
  t     :   0   1   2   3   4   5   6   7   8   9  10
  ref   :  7   0   1   2   0   3   0   4   2   3   0
         --------------------------------------------
  f0    :   7   7   7   2   2   2   2   4   4   4   0
  f1    :   .   0   0   0   0   3   3   3   2   2   2
  f2    :   .   .   1   1   1   1   0   0   0   3   3
         --------------------------------------------
  fault :   F   F   F   F   .   F   F   F   F   F   F
```

Count the `F`s and you have your fault count. This is the table to put in your
report and to walk through in your presentation.

## A warning about `uses` and `age`

The library updates every bookkeeping field for you, on hits as well as on
faults, **before** `choose_victim()` is called. Do not update them yourself.
If you also increment `uses` or shift `age`, every count will be wrong and so
will every number in your report.
