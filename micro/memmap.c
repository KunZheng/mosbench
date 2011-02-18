/*
 * Create threads, call mmap and munmap
 */

#define TESTNAME "memmap"

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

#include <pthread.h>

#include "bench.h"
#include "gemaphore.h"
#include "support/mtrace.h"

#define NPMC 3

#define MAX_PROC 256

static uint64_t start;
static uint64_t nbytes;

static unsigned int ncores;
static unsigned int the_time;
static int mmap_flags;

static uint64_t pmc_start[NPMC];
static uint64_t pmc_stop[NPMC];

static struct {
	union __attribute__((__aligned__(64))){
		volatile uint64_t v;
		char pad[64];
	} count[MAX_PROC];
	volatile int run;
	struct gemaphore gema;
} *shared;

static void sighandler(int x)
{
	struct mtrace_appdata_entry entry;
	float sec, rate, one;
	uint64_t stop, tot;
	unsigned int i;

	tot = 0;
	for (i = 0; i < ncores; i++)
		tot += shared->count[i].v;

	entry.u64 = tot;
	mtrace_appdata_register(&entry);
	mtrace_enable_set(0, TESTNAME);

	stop = usec();
	shared->run = 0;

	sec = (float)(stop - start) / 1000000;
	rate = (float)tot / sec;
	one = (float)(stop - start) / (float)tot;

	printf("rate: %f per sec\n", rate);
	printf("lat: %f usec\n", one);

	for (i = 0; i < NPMC; i++)
		pmc_stop[i] = read_pmc(i);

	for (i = 0; i < NPMC; i++) {
		rate = (float)(pmc_stop[i] - pmc_start[i]) / 
			(float) shared->count[0].v;
		printf("pmc(%u): %f per op\n", i, rate);
	}
}

static void test(unsigned int core)
{
	setaffinity(core % ncores);

	if (core == 0) {
		unsigned int i;
		
		gemaphore_p(&shared->gema);

		if (signal(SIGALRM, sighandler) == SIG_ERR)
			die("signal failed\n");
		alarm(the_time);
		start = usec();

		for (i = 0; i < NPMC; i++)
			pmc_start[i] = read_pmc(i);

		mtrace_enable_set(1, TESTNAME);
		shared->run = 1;
	} else {
		gemaphore_v(&shared->gema);
		while (shared->run == 0)
			__asm __volatile ("pause");
	}

	while (shared->run) {
		void *ptr = mmap(0, nbytes, PROT_READ|PROT_WRITE,
				 mmap_flags, 0, 0);
		if (ptr == MAP_FAILED)
			edie("mmap");
		if (munmap(ptr, nbytes) < 0)
			edie("munmap");
		shared->count[core].v++;
	}
}

static void *worker(void *x)
{
	test((int)(intptr_t)x);
	return NULL;
}

static void initshared(void)
{
	shared = mmap(0, sizeof(*shared), PROT_READ|PROT_WRITE, 
		      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared == MAP_FAILED)
		die("mmap failed");
	gemaphore_init(&shared->gema, ncores - 1);
}

int main(int ac, char **av)
{
	int use_threads;
	unsigned int i;
	if (ac < 6)
		die("usage: %s time ncores num-kbytes use-threads use-populate", av[0]);

	the_time = atoi(av[1]);
	ncores = atoi(av[2]);
	nbytes = atoi(av[3]) * 1024;
	use_threads = atoi(av[4]);
	mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS | (atoi(av[5]) ? MAP_POPULATE : 0);

	initshared();

	for (i = 1; i < ncores; i++) {
		if (use_threads) {
			pthread_t th;
			if (pthread_create(&th, NULL, worker, (void *)(intptr_t)i) < 0)
				edie("pthread_create");
		} else {
			pid_t p;
			p = fork();
			if (p < 0)
				edie("fork");
			else if (p == 0) {
				test(i);
				return 0;
			}
		}
	}

	test(0);
	return 0;
}
