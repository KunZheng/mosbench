/*
 * ping-pong a process between two cores
 */

#define TESTNAME "proc_ping_pong"

#include <sys/syscall.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "bench.h"
#include "support/mtrace.h"

#define NPMC 2

/*
 * [num counters]
 */
static uint64_t pmccount[NPMC];

/*
 * [core][num counters]
 */
static uint64_t pmclast[2][NPMC];

static uint64_t start;
static uint64_t tot;

static void __noret__ sighandler(int x)
{
	uint64_t stop;
	float sec;
	float rate;
	float pmcrate;
	float one;
	int i;

	mtrace_enable_set(0, TESTNAME, sizeof(TESTNAME));
	
	stop = usec();
	
	sec = (float)(stop - start) / 1000000;
	rate = (float)tot / sec;
	one = (float)(stop - start) / (float)tot;

	printf("rate: %f per sec\n", rate);
	printf("lat: %f usec\n", one);
	
	for (i = 0; i < NPMC; i++) {
		pmcrate = (float)(pmccount[i]) / (float)tot;
		printf("pmc %u: %f per mig\n", i, pmcrate);
	}

	exit(EXIT_SUCCESS);
}

int main(int ac, char **av)
{
	int time;
	int c[2];

	if (ac != 4)
		die("usage: %s cpu0 cpu1 time\n", av[0]);
	
	c[0] = atoi(av[1]);
	c[1] = atoi(av[2]);
	time = atoi(av[3]);
	
	setaffinity(c[1]);

	if (signal(SIGALRM, sighandler) == SIG_ERR)
		die("signal failed\n");
	alarm(time);

	mtrace_enable_set(1, TESTNAME, sizeof(TESTNAME));
	start = usec();
	for (; ; tot++) {
		uint64_t pmc;
		int a, i;

		a = tot % 2;
		setaffinity(c[a]);
		
		for (i = 0; i < NPMC; i++) {
			pmc = read_pmc(i);
			if (pmclast[a][i])
				pmccount[i] += pmc - pmclast[a][i];
			pmclast[a][i] = pmc;
		}			
	}
	return 0;
}
