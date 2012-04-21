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
    	    volatile double tput;
	};
	char pad[CACHE_BYTES];
    } cpu[MAX_CPU] __attribute__((aligned(CACHE_BYTES)));
} *sync_state;

static int npg = 1;
static int reference = 1;
static int niter;
static int ncores;

static void *
worker(void *x)
{
	int i = (uintptr_t)x;
	uint64_t s;
	int k, j;
	uint64_t t;

	setaffinity(i);
	sync_state->cpu[i].ready = 1;
	if (i)
		while (!sync_state->start)
			nop_pause();
	else
		sync_state->start = 1;
	
	s = read_tsc();
	t = usec();

	for (k = 0; k < niter; k++) {
	  volatile char *p = (char*) (0x100000000UL + i * npg * 0x100000);

	  if (mmap((void *) p, npg * 4096, PROT_READ|PROT_WRITE,
		   MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
	    fprintf(stderr, "%d: map failed\n", i);
	    exit(-1);
	  }
	  
	  if (reference) {
	    for (j = 0; j < npg * 4096; j++)
	      p[j] = '\0';
	  }

	  if (munmap((void *) p, npg * 4096) < 0) {
	    fprintf(stderr, "%d: unmap failed\n", i);
	    exit(-1);
	  }

	}
	
	sync_state->cpu[i].cycle = read_tsc() - s;
	double sec = (usec() - t) / 1000000.0;
	sync_state->cpu[i].tput = (double) niter / sec;
	return 0;
}

static void waitup(void)
{
        uint64_t tot, max;
	int i;
	double avg = 0.0;

	tot = 0;
	max = 0;
	for (i = 0; i < ncores; i++) {
		while (!sync_state->cpu[i].cycle)
			nop_pause();

		tot += sync_state->cpu[i].cycle;
		if (sync_state->cpu[i].cycle > max)
			max = sync_state->cpu[i].cycle;
		avg += sync_state->cpu[i].tput;
	}

	printf("%d cores completed %d map+unmap in %"PRIu64" cycles %lu tput %.3g\n", 
	       ncores, niter, tot, avg/ncores);
}

int main(int ac, char **av)
{
	int threads;
	pthread_t th;
	int i;

	setaffinity(0);
	if (ac < 3)
		die("usage: %s num-cores niter use-threads", av[0]);
	
	ncores = atoi(av[1]);
	niter = atoi(av[2]);
	threads = atoi(av[3]);

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
