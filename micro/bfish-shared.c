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
	lat = (float)(stop - shared->start) / (float)shared->count[0].v;

	printf("rate %f per cyc\n", rate);
	printf("ave lat %f cyc\n", lat);
}

static void * workloop(void *x)
{
	int c = (long)x;
	int i;
	uint64_t gen;
	char *b;
	
	setaffinity(c);
	memset(shared->clines, 0, nclines * CLINESZ);

	b = shared->clines;

	if (c) {
		while (shared->go == 0)
			__asm __volatile ("pause");
	} else {
		if (signal(SIGALRM, sighandler) == SIG_ERR)
			edie("signal failed\n");
		alarm(the_time);
		
		shared->start = read_tsc();
		shared->go = 1;
	}

	gen = 0;
	switch (nclines) {
	case 1:
		for (; shared->go;) {
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b));
			shared->count[c].v++;
		}
		break;
	case 2:
		for (; shared->go;) {
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 64));
			shared->count[c].v++;
		}
		break;
	case 4:
		for (; shared->go;) {
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 64));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 128));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 192));
			shared->count[c].v++;
		}
		break;
	case 8:
		for (; shared->go;) {
			//prefetchw(b + 64);
			//prefetchw(b + 128);
			//prefetchw(b + 256);
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 64));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 128));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 192));
			//prefetchw(b + 320);
			//prefetchw(b + 384);
			//prefetchw(b + 448);

			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 256));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 320));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 384));
			__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + 448));

			shared->count[c].v++;
		}
		break;
	default:
		while (shared->go) {
			for (i = 0; i < nclines; i++) {
				__asm__ __volatile__("movq %%rax, (%0)" :: "r" (b + (i * CLINESZ)));
			}
			shared->count[c].v++;
		}
		break;
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

	for (i = 1; i < nthreads; i++) {
		pthread_t th;
		if (pthread_create(&th, 0, workloop, (void *)(long)i) < 0)
			edie("pthread_create failed");
	}

	sleep(1);
	shared->signal[0].v = 1;
	workloop(0);

	return 0;
}
