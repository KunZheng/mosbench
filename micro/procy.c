/*
 * Create task per-core, each task does something
 */

#define TESTNAME "fops_dir"

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
#include "argv.h"

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
	float sec, rate, one;
	uint64_t stop, tot;
	unsigned int i;

	for (i = 0; i < NPMC; i++)
		pmc_stop[i] = read_pmc(i);

	stop = usec();
	shared->run = 0;

	tot = 0;
	for (i = 0; i < ncores; i++)
		tot += shared->count[i].v;

	sec = (float)(stop - start) / 1000000;
	rate = (float)tot / sec;
	one = (float)(stop - start) / (float)tot;

	printf("rate: %f per sec\n", rate);
	printf("lat: %f usec\n", one);

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

static struct args the_args;
static const char *valid_args[] = { "time", "ncores", "use_threads", "sched_op", NULL };

int main(int ac, char **av)
{
	int use_threads;
	unsigned int i;
	if (ac < 5)
		die("usage: %s time ncores use-threads operation", av[0]);

	if (argv_parse(ac, av, &the_args, valid_args) != 4)
		argv_usage(&the_args);
	argv_print(&the_args);

	the_time = atoi(av[1]);
	ncores = atoi(av[2]);
	use_threads = atoi(av[3]);
	//the_op = atoi(av[4]);

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
