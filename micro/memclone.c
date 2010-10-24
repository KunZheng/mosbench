/*
 * Create threads, touch memory, and cause the kernel to demand allocate pages.
 */

#include <sys/mman.h>
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
static unsigned int page_size;

static void *
worker(void *x)
{
	int i = (uintptr_t)x;
	char *buf, *end;
	uint64_t s;

	setaffinity(i);
	buf = mmap(0, nbytes, PROT_READ | PROT_WRITE, 
		   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED)
		die("mmap failed");
	end = buf + nbytes;
	
	sync_state->cpu[i].ready = 1;
	if (i)
		while (!sync_state->start)
			nop_pause();
	else
		sync_state->start = 1;
	
	s = read_tsc();
	for (; buf != end; buf += page_size)
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

	ave_pf = (tot / ncores) / (nbytes / page_size);
	max_pf = max / (nbytes / page_size);
	printf("ave cycles/pf %"PRIu64", max cycles/pf %"PRIu64"\n", 
	       ave_pf, max_pf);
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

	page_size = get_page_size();

	sync_state = (void *) mmap(0, sizeof(*sync_state), 
				   PROT_READ | PROT_WRITE, 
				   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (sync_state == MAP_FAILED)
		edie("mmap sync_state failed");
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
