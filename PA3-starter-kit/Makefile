CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

all: vmsim memlab memlab-static

# Part 2 and Part 3: the page replacement simulator.
vmsim: vmsim.c vmlib.c vmlib.h vm.h
	$(CC) $(CFLAGS) -o $@ vmsim.c vmlib.c

# Part 1, built twice. These two binaries contain the same program.
# The only difference is where the C library comes from: slide 31.
memlab: memlab.c
	$(CC) $(CFLAGS) -o $@ memlab.c

memlab-static: memlab.c
	$(CC) $(CFLAGS) -static -o $@ memlab.c

# The static against dynamic comparison your report has to include.
sizes: memlab memlab-static
	@echo "=== static against dynamic linking (lecture slide 31) ==="
	@ls -lh memlab memlab-static
	@echo
	@echo "--- shared libraries the DYNAMIC build needs at run time ---"
	@ldd ./memlab || true
	@echo
	@echo "--- and the STATIC build ---"
	@ldd ./memlab-static 2>&1 | sed "s/^/\t/" || true
	@echo "\t(nothing, or 'not a dynamic executable': the C library is"
	@echo "\t already inside the binary, which is why it is so much bigger)"

run: vmsim
	./vmsim refs/r1-lecture.txt -f 3

test: vmsim
	@for f in refs/r*.txt; do \
		echo "=== $$f, 3 frames ==="; \
		./vmsim $$f -f 3 | grep -E "Page faults|Fault rate"; \
	done
	@echo "=== refs/r7-reuse.txt, 4 frames (the one that needs 4) ==="
	@./vmsim refs/r7-reuse.txt -f 4 | grep -E "Page faults|Fault rate"

clean:
	rm -f vmsim memlab memlab-static paging_report.csv

.PHONY: all sizes run test clean
