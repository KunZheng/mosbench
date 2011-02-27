/*
 * Bind to a core, perform some operation
 */

#define TESTNAME "exec_client"

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
#include "support/mtrace.h"
#include "gemaphore.h"
#include "argv.h"

static void (*op_fn)(unsigned int core);
static uint64_t op_count;
static uint64_t start;

static struct args the_args = {
	.coreid = 1,
	.exec_op = "create-proc",
	.mtrace_captain = 0
};

static const char *valid_args[] = 
       { "coreid", "exec_op", "mtrace_captain", NULL };

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
	if (!strcmp(the_args.exec_op, "create-thread"))
		op_fn = create_thread_op;
	else if (!strcmp(the_args.exec_op, "create-proc"))
		op_fn = create_proc_op;
	else
		die("bad sched_op: %s", the_args.exec_op);
}

static void __noret__ print_result(int x)
{
	uint64_t stop;
	float rate;
	float sec;

	mtrace_enable_set(0, TESTNAME);

	stop = usec();
	sec = (float)(stop - start) / 1000000;
	rate = (float)op_count / sec;
	printf("rate: %f per sec\n", rate);

	exit(EXIT_SUCCESS);
}

static void reset(int x)
{
	op_count = 0;
	start = usec();
	if (the_args.mtrace_captain)
		mtrace_enable_set(1, TESTNAME);
}

int main(int ac, char **av)
{
	argv_parse(ac, av, &the_args, valid_args);

	set_op_fn();

	if (signal(SIGUSR1, reset) == SIG_ERR)
		edie("signal(SIGUSR1)");
	if (signal(SIGUSR2, print_result) == SIG_ERR)
		edie("signal(SIGUSR2)");

	setaffinity(the_args.coreid);

	for(;;) {
		op_fn(the_args.coreid);
		op_count++;
	}
	return 0;
}
