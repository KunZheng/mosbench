/*
 * hold a lock for some cycles, spin for some cycles, repeat
 */
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "bench.h"
#include "spinlock.h"
#include "list.h"

static spinlock_t lock = SPIN_LOCK_UNLOCKED;
static unsigned int ncores;
static unsigned int the_time;
static unsigned int lock_cycles;
static unsigned int unlock_cycles;
static volatile int go;

static uint64_t start;

struct counter {
	volatile uint64_t v;
	char pad[0] __attribute__((aligned(64)));
};
static struct counter *the_counter;

struct linky {
	struct list_head link;
	char pad[0] __attribute__((aligned(64)));
};
static struct linky *the_linky;
static struct list_head the_list;

static unsigned int id2core[] = {
	0,  1,  2,  3,  4,  5,
	6,  7,  8,  9,  10, 11,
	12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35,
	36, 37, 38, 39, 40, 41,
	42, 43, 44, 45, 46, 47
};

#if 0
static unsigned int id2core[] = {
	0,  24,  6,  30,  12,  36,
	6,  7,  8,  9,  10, 11,
	12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35,
	36, 37, 38, 39, 40, 41,
	42, 43, 44, 45, 46, 47
};
#endif

static void __noret__ sighandler(int x)
{
	uint64_t stop, tot;
	unsigned int i;
	float sec;
	float rate;
	float one;

	go = 2;	
	stop = usec();

	tot = 0;
	for (i = 0; i < ncores; i++)
		tot += the_counter[i].v;
	
	sec = (float)(stop - start) / 1000000;
	rate = (float)tot / sec;
	one = (float)(stop - start) / (float)tot;

	printf("rate: %f per sec\n", rate);
	printf("lat: %f usec\n", one);

	exit(EXIT_SUCCESS);
}

static void spin(uint64_t cycles)
{
	uint64_t s = read_tsc();
	while ((read_tsc() - s ) < cycles)
		cpu_relax();
}

static void *worker(void *x)
{
	struct counter *c;
	unsigned int id; 

	id = (long)x;
	c = &the_counter[id];
	setaffinity(id2core[id]);
	
	if (id == 0) {
		if (signal(SIGALRM, sighandler) == SIG_ERR)
			die("signal failed\n");
		alarm(the_time);
		start = usec();
		go = 1;
	} else {
		while (go == 0)
			cpu_relax();
		spin(1000 * id);
	}

	while (go == 1) {
		spin(unlock_cycles);
		spin_lock(&lock);
		list_del(the_list.next);
		list_add(&the_linky[id].link, &the_list);
		//spin(lock_cycles);
		spin_unlock(&lock);

		c->v++;
	}

	return NULL;
}

int main(int ac, char **av)
{
	unsigned int i;
	pthread_t *th;
	int r;

	if (ac < 5)
		die("usage: %s ncores time lock-cycles unlock-cycles", av[0]);
	
	ncores = atoi(av[1]);
	the_time = atoi(av[2]);
	lock_cycles = atoi(av[3]);
	unlock_cycles = atoi(av[4]);

	the_counter = xmalloc(sizeof(*the_counter) * ncores);
	the_linky = xmalloc(sizeof(*the_linky) * ncores);

	INIT_LIST_HEAD(&the_list);
	list_add(&the_linky[0].link, &the_list);

	th = xmalloc(sizeof(pthread_t) * ncores);

	for (i = 1; i < ncores; i++) {
		r = pthread_create(&th[i], NULL, worker, (void*)(long)i);
		if (r)
			edie("pthread_create");
	}

	if (i > 1)
		sleep(1);
	worker(0);

	free(the_counter);
	return 0;
}
