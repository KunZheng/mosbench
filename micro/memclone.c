#include <sys/mman.h>
#include <sys/user.h>
#include <stdio.h>
#include <pthread.h>

#include "bench.h"

static struct {
    volatile int start;
    union {
	struct {
	    volatile int ready;
	    volatile uint64_t cycle;
	};
	char pad[CACHE_BYTES];
    } cpu[MAX_CPU] __attribute__((aligned(CACHE_BYTES)));
} *sync_state;

static int nbytes;
static int ncores;

static void *
worker(void *x)
{
	int i = (uintptr_t)x;
	setaffinity(i);
	char *buf = malloc(nbytes);
	if (!buf)
		edie("malloc failed");
	char *end = buf + nbytes;
	
	sync_state->cpu[i].ready = 1;
	if (i)
		while (!sync_state->start)
			nop_pause();
	else
		sync_state->start = 1;
	
	uint64_t s = read_tsc();
	for (; buf != end; buf += PAGE_SIZE)
		*buf = 1;
	
	sync_state->cpu[i].cycle = read_tsc() - s;
	return 0;
}

static void waitup(void)
{
	uint64_t ave_pf, max_pf;
	uint64_t tot, max;
	int i;

	tot = 0;
	max = 0;
	for (i = 0; i < ncores; i++) {
		while (!sync_state->cpu[i].cycle)
			nop_pause();
		tot += sync_state->cpu[i].cycle;
		if (sync_state->cpu[i].cycle > max)
			max = sync_state->cpu[i].cycle;
	}
	
	ave_pf = (tot / ncores) / (nbytes / PAGE_SIZE);
	max_pf = max / (nbytes / PAGE_SIZE);
	printf("ave cycles/pf %ld, max cycles/pf %ld\n", ave_pf, max_pf);
}

int main(int ac, char **av)
{
	int threads;
	pthread_t th;
	int i;

	setaffinity(0);
	if (ac < 3)
		die("usage: %s num-cores num-mbytes use-threads", av[0]);
	
	ncores = atoi(av[1]);
	nbytes = atoi(av[2]) * 1024 * 1024;
	threads = atoi(av[3]);

	sync_state = mmap(0, sizeof(*sync_state), PROT_READ | PROT_WRITE, 
			  MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (sync_state == MAP_FAILED)
		die("mmap failed");
	memset(sync_state, 0, sizeof(*sync_state));
    
	for (i = 1; i < ncores; i++) {
		if (threads) {
			if (pthread_create(&th, NULL, worker, (void *)(intptr_t)i) < 0)
				edie("pthread_create");
		} else {
			pid_t p = fork();
			if (p < 0)
				edie("fork");
			if (!p) {
				worker((void *)(intptr_t)i);
				return 0;
			}
		}
		
		while (!sync_state->cpu[i].ready)
			nop_pause();
	}

	worker((void *)(intptr_t)0);
	waitup();
	return 0;
}
