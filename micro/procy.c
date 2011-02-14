/*
 * Create task per-core, each task does something
 */

#define TESTNAME "procy"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <signal.h>

#include <pthread.h>

#include "bench.h"
#include "gemaphore.h"
#include "argv.h"

#define NPMC 3

#define MAX_PROC 1024

static uint64_t start;

static uint64_t pmc_start[NPMC];
static uint64_t pmc_stop[NPMC];

static struct args the_args = {
	.time = 5,
	.ncores = 1,
	.nprocs = 1,
	.use_threads = 0,
	.sched_op = "create-proc",
};

static const char *valid_args[] = 
       { "time", "ncores", "nprocs", "use_threads", "sched_op", NULL };

static void (*op_fn)(unsigned int core);

static struct {
	union __attribute__((__aligned__(64))){
		volatile uint64_t v;
		char pad[64];
	} count[MAX_PROC];
	volatile int run;
	struct gemaphore gema;
} *shared;

static void __noret__ sighandler(int x)
{
	float sec, rate, one;
	uint64_t stop, tot;
	unsigned int i;

	for (i = 0; i < NPMC; i++)
		pmc_stop[i] = read_pmc(i);

	stop = usec();
	shared->run = 0;
	kill(0, SIGTERM);

	tot = 0;
	for (i = 0; i < the_args.nprocs; i++)
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

	exit(EXIT_SUCCESS);
}

static void test(unsigned int proc)
{
	setaffinity(proc % the_args.ncores);

	if (proc == 0) {
		unsigned int i;
		
		gemaphore_p(&shared->gema);

		if (signal(SIGALRM, sighandler) == SIG_ERR)
			edie("signal(SIGALRM)");
		if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
			edie("signal(SIGKILL)");
			
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
	while (shared->run) {
		op_fn(proc);
		shared->count[proc].v++;
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
	gemaphore_init(&shared->gema, the_args.nprocs - 1);
}

static void sleep_op(unsigned int proc) 
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000;
	nanosleep(&ts, NULL);
}

static void *null_worker(void *x)
{
	return NULL;
}

static void create_thread_op(unsigned int proc)
{
	pthread_t th;

	if (pthread_create(&th, NULL, null_worker, NULL) < 0)
		edie("pthread_create");
	pthread_join(th, NULL);
}

static void create_proc_op(unsigned int proc)
{
	pid_t p = fork();
	if (p < 0)
		edie("fork");
	else if (p == 0)
		exit(EXIT_SUCCESS);
	if (wait(NULL) < 0)
		edie("wait");
}

static void set_op_fn(void)
{
	if (!strcmp(the_args.sched_op, "sleep"))
		op_fn = sleep_op;
	else if (!strcmp(the_args.sched_op, "create-thread"))
		op_fn = create_thread_op;
	else if (!strcmp(the_args.sched_op, "create-proc"))
		op_fn = create_proc_op;
	else
		die("bad sched_op: %s", the_args.sched_op);
}

int main(int ac, char **av)
{
	unsigned int i;

	argv_parse(ac, av, &the_args, valid_args);

	set_op_fn();
	initshared();

	for (i = 1; i < the_args.nprocs; i++) {
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
