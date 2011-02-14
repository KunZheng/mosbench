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

static uint64_t pmc_start[NPMC];
static uint64_t pmc_stop[NPMC];

static struct args the_args;
static const char *valid_args[] = 
       { "time", "ncores", "use_threads", "sched_op", NULL };

static void (*op_fn)(unsigned int core);

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
	for (i = 0; i < the_args.ncores; i++)
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
	setaffinity(core % the_args.ncores);

	if (core == 0) {
		unsigned int i;
		
		gemaphore_p(&shared->gema);

		if (signal(SIGALRM, sighandler) == SIG_ERR)
			die("signal failed\n");
		alarm(the_args.time);
		start = usec();

		for (i = 0; i < NPMC; i++)
			pmc_start[i] = read_pmc(i);

		shared->run = 1;
	} else {
		gemaphore_v(&shared->gema);
		while (shared->run == 0)
			__asm __volatile ("pause");
	}

	op_fn(core);
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
	gemaphore_init(&shared->gema, the_args.ncores - 1);
}

static void sleep_op(unsigned int core) 
{
	while (shared->run) {
		shared->count[core].v++;
	}
}

static void create_op(unsigned int core)
{
	while (shared->run) {
		shared->count[core].v++;
	}
}

static void set_op_fn(void)
{
	if (!strcmp(the_args.sched_op, "sleep"))
		op_fn = sleep_op;
	else if (!strcmp(the_args.sched_op, "create"))
		op_fn = create_op;
	else
		die("bad sched_op: %s", the_args.sched_op);
}

int main(int ac, char **av)
{
	unsigned int i;

	if (argv_parse(ac, av, &the_args, valid_args) != 4)
		argv_usage(&the_args);
	set_op_fn();
	initshared();

	for (i = 1; i < the_args.ncores; i++) {
		if (the_args.use_threads) {
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
