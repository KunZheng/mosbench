/*
 * atomic inc as fast as possible
 */

#define TESTNAME "icnt"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include "bench.h"
#include "support/mtrace.h"
#include "atomic.h"

#define GHZ 2.411529f
#define NPMC 3
#define MAX_PROC 256

static uint64_t start;

static unsigned int ncores;
static unsigned int nclines;
static unsigned int the_time;
static unsigned int max_ndx;

static uint64_t pmc_start[NPMC];
static uint64_t pmc_stop[NPMC];

static struct {
	union __attribute__((__aligned__(64))){
		volatile uint64_t v;
		char pad[64];
	} count[MAX_PROC];
	volatile int run;
	
	atomic_t *buffer;
} *shared;

static void sighandler(int x)
{
	double cyc, rate, lat;
	uint64_t stop, tot;
	unsigned int i;

	mtrace_enable_set(0, TESTNAME);

	for (i = 0; i < NPMC; i++)
		pmc_stop[i] = read_pmc(i);

	stop = usec();
	shared->run = 0;

	tot = 0;
	for (i = 0; i < ncores; i++)
		tot += shared->count[i].v;

	cyc = (double)(stop - start) * 1000 * GHZ;
	rate = (double)tot / cyc;
	lat = (1.0f / rate) * (float)ncores;

	printf("rate: %f per cycles\n", rate);
	printf("late: %f\n", lat);

	for (i = 0; i < NPMC; i++) {
		rate = (double)(pmc_stop[i] - pmc_start[i]) / 
			(double) shared->count[0].v;
		printf("pmc(%u): %f per op\n", i, rate);
	}
}

static inline void op_cline(unsigned int ndx)
{
	atomic_inc(&shared->buffer[ndx]);
}

static void test(unsigned int core)
{
	register unsigned int mask;
	register uint32_t seed;

	setaffinity(core);
	seed = getpid();
	mask = max_ndx - 1;

	if (core == 0) {
		unsigned int i;

		if (signal(SIGALRM, sighandler) == SIG_ERR)
			die("signal failed\n");
		alarm(the_time);
		start = usec();

		for (i = 0; i < NPMC; i++)
			pmc_start[i] = read_pmc(i);

		mtrace_enable_set(1, TESTNAME);
		shared->run = 1;
	} else {
		while (shared->run == 0)
			__asm __volatile ("pause");
	}

	while (shared->run) {
		seed = seed * 1103515245 + 12345;
		op_cline(seed & mask);
		shared->count[core].v++;
	}
}

static void initshared(void)
{
	shared = mmap(0, sizeof(*shared), PROT_READ|PROT_WRITE, 
		      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared == MAP_FAILED)
		die("mmap failed");

	shared->buffer = mmap(0, nclines * 64, PROT_READ|PROT_WRITE, 
			      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared->buffer == MAP_FAILED)
		die("mmap failed");
	memset((void *)shared->buffer, 0, nclines * 64);
}

int main(int ac, char **av)
{
	unsigned int i;

	if (ac < 4)
		die("usage: %s time nun-cores kbytes", av[0]);

	the_time = atoi(av[1]);
	ncores = atoi(av[2]);
	nclines = (atoi(av[3]) * 1024) / 64;
	max_ndx = (nclines * 64) / sizeof(*shared->buffer);

	if (max_ndx & (max_ndx - 1))
		die("max_ndx %u not a power of 2", max_ndx);

	initshared();

	for (i = 1; i < ncores; i++) {
		pid_t p;

		p = fork();
		if (p < 0)
			edie("fork");
		else if (p == 0) {
			test(i);
			return 0;
		}
	}

	sleep(1);
	test(0);

	return 0;
}
