/*
 * Each process: create a few files in a directory, open files for a
 * number of iterations, and unlink them.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>

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

#define MAXNAME 64

static int niter;
static int ncores;
static int nfile = 10;
static char dirs[MAX_CPU][MAXNAME];

static void *
worker(void *x)
{
	int i = (uintptr_t)x;
	uint64_t s;
	int k, j;
	uint64_t t;
	char pn[MAXNAME];

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

	  for (j = 0; j < nfile; j++) {
	    snprintf(pn, sizeof(pn), "%s/f:%d:%d", dirs[i], i, j);
	    // printf("path %s\n", pn);
	    int fd = open(pn, O_CREAT | O_RDWR, S_IRUSR|S_IWUSR|S_IXUSR);
	    if (fd < 0)
	      perror("create failed");

	    close(fd);
	  }

	  for (j = 0; j < niter; j++) {
	    snprintf(pn, sizeof(pn), "%s/f:%d:%d", dirs[i], i, (j % nfile));
	    int fd = open(pn, O_RDWR);
	    if (fd < 0) {
	      perror("open failed");
	      exit(-1);
		}

	    close(fd);
	  }

	  for (j = 0; j < nfile; j++) {
	    snprintf(pn, sizeof(pn), "%s/f:%d:%d", dirs[i], i, j);
	    if (unlink(pn) < 0)
	      fprintf(stderr, "unlink failed\n");
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
	int i;

	setaffinity(0);
	if (ac < 3)
		die("usage: %s num-cores niter", av[0]);
	
	ncores = atoi(av[1]);
	niter = atoi(av[2]);

	for (i = 0; i < ncores; i++) {
	  //snprintf(dirs[i], sizeof(dirs[i]), "/db%d", i);
	  snprintf(dirs[i], sizeof(dirs[i]), "/tmp/dirbench");
	}
	if (mkdir(dirs[0], S_IRUSR|S_IWUSR|S_IXUSR) < 0)
	    fprintf(stderr, "mkdir failed\n");

	sync_state = (void *) mmap(0, sizeof(*sync_state), 
				   PROT_READ | PROT_WRITE, 
				   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (sync_state == MAP_FAILED)
		edie("mmap sync_state failed");
	memset(sync_state, 0, sizeof(*sync_state));
    
	for (i = 1; i < ncores; i++) {
	  pid_t p = fork();
	  if (p < 0)
	    edie("fork");
	  if (!p) {
	    worker((void *)(intptr_t)i);
	    return 0;
	  }
		
	  while (!sync_state->cpu[i].ready)
	    nop_pause();
	}

	worker((void *)(intptr_t)0);
	waitup();

	for (i = 0; i < 1; i++) {
	  //snprintf(dirs[i], sizeof(dirs[i]), "/db%d", i);
	  rmdir(dirs[i]);
	}

	return 0;
}
