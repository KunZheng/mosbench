#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "bench.h"

#define CLINESZ 64

static int nthreads;
static int nclines;
static int the_time;

static struct {
	union __attribute__((aligned(128))){
		volatile uint64_t v;
		char __pad[128];
	} count[48];

	union __attribute__((aligned(128))){
		volatile uint64_t v;
		char __pad[128];
	} signal[48];

	volatile int go;

	char *clines;
	
	uint64_t start;
} *shared;

static inline void prefetchw(void *a)
{
	__asm __volatile("prefetchw (%0)" : : "r" (a));
}

static void initshared(void)
{
	shared = mmap(0, sizeof(*shared), PROT_READ|PROT_WRITE, 
		      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared == MAP_FAILED)
		die("mmap failed\n");

	shared->clines = mmap(0, nclines * CLINESZ, PROT_READ|PROT_WRITE, 
			      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared == MAP_FAILED)
		die("mmap failed\n");
}

static void sighandler(int x)
{
	float rate, lat;
	uint64_t stop;
	uint64_t tot;
	int i;


	shared->go = 0;
	stop = read_tsc();

	tot = 0;
	for (i = 0; i < 48; i++)
		tot += shared->count[i].v;

	rate = (float)tot / (float)(stop - shared->start);
	lat = (float)(stop - shared->start) / (float)shared->count[1].v;

	printf("rate %f per cyc\n", rate);
	printf("ave lat %f cyc\n", lat);
}

static void * workloop(void *x)
{
	int c = (long)x;

	setaffinity(c);
	
	for (;shared->go;) {
		shared->signal[c].v = 1;
		while (shared->signal[c].v)
			__asm __volatile("pause");
		shared->count[c].v++;
	}

	return 0;
}

static void * serverloop(void *x)
{
	int c = (long)x;
	int i, flag;

	setaffinity(c);

	if (signal(SIGALRM, sighandler) == SIG_ERR)
		edie("signal failed\n");
	alarm(the_time);
		
	shared->start = read_tsc();

	for (;shared->go;) {
		flag = 0;
		for (i = 0; i < nthreads; i++) {
			if (shared->signal[i].v) {
				shared->signal[i].v = 0;
				flag = 1;
			}
		}
		if (flag == 0)
			__asm __volatile("pause");
	}

	return 0;
}

int main(int ac, char **av)
{
	int i;

	if (ac != 4)
		die("usage: %s nthreads nclines the_time\n", av[0]);
	
	nthreads = atoi(av[1]);
	nclines = atoi(av[2]);
	the_time = atoi(av[3]);

	setaffinity(0);

	initshared();
	shared->go = 1;

	for (i = 1; i < nthreads; i++) {
		pthread_t th;
		if (pthread_create(&th, 0, workloop, (void *)(long)i) < 0)
			edie("pthread_create failed");
	}

	sleep(1);
	shared->signal[0].v = 1;
	serverloop(0);

	return 0;
}
