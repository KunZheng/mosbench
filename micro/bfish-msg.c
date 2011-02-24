#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "bench.h"
#include "atomic.h"
#include "bfish.h"

#define CLINESZ 64

static int nthreads;
static int nservers;
static int nclines;
static int nsets;
static int the_time;

static int sets_per_server;

union msgbuf {
	volatile uint64_t v;
	char __pad[128];
};

struct signaler {
	union msgbuf mb[48] __attribute__((aligned(128)));
};

static struct {
	union __attribute__((aligned(128))){
		volatile uint64_t v;
		char __pad[128];
	} count[48];

	struct signaler signaler[48];

	volatile int go;

	char **clines;

	uint64_t start;
} *shared;

static inline void prefetchw(void *a)
{
	__asm __volatile("prefetchw (%0)" : : "r" (a));
}

static void initshared(void)
{
	int i;

	shared = mmap(0, sizeof(*shared), PROT_READ|PROT_WRITE, 
		      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared == MAP_FAILED)
		die("mmap failed");

	shared->clines = mmap(0, nsets * sizeof(*shared->clines), 
			      PROT_READ|PROT_WRITE, 
			      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared->clines == MAP_FAILED)
		die("mmap failed");

	for (i = 0; i < nsets; i++) {
		shared->clines[i] = mmap(0, nclines * CLINESZ, PROT_READ|PROT_WRITE, 
					 MAP_SHARED|MAP_ANONYMOUS, 0, 0);
		if (shared->clines[i] == MAP_FAILED)
			die("mmap failed");
		memset(shared->clines[i], 0, nclines * CLINESZ);
	}
}

static void sighandler(int x)
{
	float rate, sec;
	uint64_t stop;
	uint64_t tot;
	int i;

	shared->go = 0;
	stop = usec();

	tot = 0;
	for (i = 0; i < 48; i++)
		tot += shared->count[i].v;

	sec = (float)(stop - shared->start) / 1000000;
	rate = (float)tot / sec;

	printf("rate %f per sec\n", rate);
}

static void * workloop(void *x)
{
	int c = (long)x;
	uint32_t seed;

	seed = c;
	setaffinity(c);

	while (shared->go == 0)
		__asm __volatile ("pause");

	for (;shared->go;) {
		int server = rnd(&seed) % nservers;

		shared->signaler[server].mb[c].v = 1;
		while (shared->signaler[server].mb[c].v)
			__asm __volatile("pause");
		shared->count[c].v++;
	}

	return 0;
}

static void do_op(int set)
{
	char *b;
	int i;

	b = shared->clines[set];

	switch (nclines) {
	case 1:
		XOP(b);
		break;
	case 2:
		XOP(b);
		XOP(b + 64);
		break;
	case 4:
		XOP(b);
		XOP(b + 64);
		XOP(b + 128);
		XOP(b + 192);
		break;
	case 8:
		XOP(b);
		XOP(b + 64);
		XOP(b + 128);
		XOP(b + 192);
		XOP(b + 256);
		XOP(b + 320);
		XOP(b + 384);
		XOP(b + 448);
		break;
	default:
		for (i = 0; i < nclines; i++) {
			XOP(b);
			b += 64;
		}
		break;
	}
}

static void * serverloop(void *x)
{
	int c = (long)x;
	uint32_t seed;
	int i, flag;
	int set_offset;

	setaffinity(c);
	seed = c;
	set_offset = c * sets_per_server;

	if (c == 0) {
		if (signal(SIGALRM, sighandler) == SIG_ERR)
			edie("signal failed\n");
		alarm(the_time);
		
		shared->start = usec();
		shared->go = 1;
	} else {
		while (shared->go == 0)
			__asm __volatile ("pause");
	}

	for (;shared->go;) {
		flag = 0;
		for (i = 0; i < nthreads; i++) {
			int worker = nservers + i;
			if (shared->signaler[c].mb[worker].v) {
				int set = (rnd(&seed) % sets_per_server) + set_offset;

				do_op(set);
				shared->signaler[c].mb[worker].v = 0;
				flag = 1;
			}
		}
		__asm __volatile("pause");
	}

	return 0;
}

int main(int ac, char **av)
{
	int i;

	if (ac != 6)
		die("usage: %s nthreads nservers nclines nsets the_time", av[0]);
	
	nthreads = atoi(av[1]);
	nservers = atoi(av[2]);
	nclines = atoi(av[3]);
	nsets = atoi(av[4]);
	the_time = atoi(av[5]);

	if (nsets % nservers)
		die("make nservers a multiple of nsets");

	sets_per_server = nsets / nservers;

	setaffinity(0);

	initshared();

	for (i = 1; i < nservers; i++) {
		pthread_t th;
		if (pthread_create(&th, 0, serverloop, (void *)(long)i) < 0)
			edie("pthread_create failed");
	}

	for (i = 0; i < nthreads; i++) {
		pthread_t th;
		if (pthread_create(&th, 0, workloop, (void *)(long)(i + nservers)) < 0)
			edie("pthread_create failed");
	}

	sleep(1);
	serverloop(0);

	return 0;
}
