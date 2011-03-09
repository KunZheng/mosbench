/*
 * Create task per-core, each task does something
 */

#define TESTNAME "nety"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <signal.h>

#include <pthread.h>

#include "bench.h"
#include "gemaphore.h"
#include "argv.h"
#include "support/mtrace.h"

#define NPMC 3
#define MAX_CORE 64
#define THE_PORT 4321

static uint64_t start;

static uint64_t pmc_start[NPMC];
static uint64_t pmc_stop[NPMC];

static struct args the_args = {
	.time = 5,
	.ncores = 1,
};

static const char *valid_args[] = 
       { "time", "ncores", NULL };

static struct {
	union __attribute__((__aligned__(64))){
		volatile uint64_t v;
		char pad[64];
	} count[MAX_CORE];
	volatile int run;
	struct gemaphore gema;
} *shared;

static void sighandler(int x)
{
	struct mtrace_appdata_entry entry;
	float sec, rate, one;
	uint64_t stop, tot;
	unsigned int i;

	tot = 0;
	for (i = 0; i < the_args.ncores; i++)
		tot += shared->count[i].v;

	entry.u64 = tot;
	mtrace_appdata_register(&entry);
	mtrace_enable_set(0, TESTNAME);

	stop = usec();
	shared->run = 0;

	sec = (float)(stop - start) / 1000000;
	rate = (float)tot / sec;
	one = (float)(stop - start) / (float)tot;

	printf("rate: %f per sec\n", rate);
	printf("lat: %f usec\n", one);

	for (i = 0; i < NPMC; i++)
		pmc_stop[i] = read_pmc(i);

	for (i = 0; i < NPMC; i++) {
		rate = (float)(pmc_stop[i] - pmc_start[i]) / 
			(float) shared->count[0].v;
		printf("pmc(%u): %f per op\n", i, rate);
	}
}

static void client(unsigned int port)
{
	struct hostent* hostinfo;
	struct sockaddr_in dst;
	struct sockaddr_in src;
	char buf[1];
	int one = 1;
	int c;

	c = socket(AF_INET, SOCK_DGRAM, 0);
	if (c < 0)
		edie("socket");
	if (setsockopt(c, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)))
		edie("setsockopt");

	src.sin_addr.s_addr = INADDR_ANY;
	src.sin_port = htons(port + MAX_CORE);
	src.sin_family = AF_INET;
	if (bind(c, (struct sockaddr*)&src, sizeof(src)))
		edie("bind");

	hostinfo = gethostbyname("127.0.0.1");
	memcpy(&dst.sin_addr.s_addr, hostinfo->h_addr, hostinfo->h_length);
	dst.sin_port = htons(port);
	dst.sin_family = AF_INET;

	for (;;) {
		int r;
		
		r = sendto(c, buf, sizeof(buf), 0, 
			   (struct sockaddr*)&dst, sizeof(dst));
		if (r < 0)
			edie("sendto");
		else if (r == 0)
			break;

		r = recv(c, buf, sizeof(buf), 0);
		if (r < 0)
			edie("recv");
		else if (r == 0)
			break;

		if (!shared->run)
			break;
	}

	sendto(c, buf, sizeof(buf), 0, 
	       (struct sockaddr*)&dst, sizeof(dst));

	close(c);
}

static void test(unsigned int core)
{
	struct hostent* hostinfo;
	struct sockaddr_in dst;
	struct sockaddr_in src;
	char buf[1] = { 0 };
	pid_t p;
	int one = 1;
	int s;
	int c;

	setaffinity(core % the_args.ncores);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		edie("socket");
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)))
		edie("setsockopt");
	
	src.sin_addr.s_addr = INADDR_ANY;
	src.sin_port = htons(THE_PORT + core);
	src.sin_family = AF_INET;
	if (bind(s, (struct sockaddr*)&src, sizeof(src)))
		edie("bind");

	hostinfo = gethostbyname("127.0.0.1");
	memcpy(&dst.sin_addr.s_addr, hostinfo->h_addr, hostinfo->h_length);
	dst.sin_port = htons(THE_PORT + core + MAX_CORE);
	dst.sin_family = AF_INET;

	p = fork();
	if (p == 0) {
		close(s);
		client(THE_PORT + core);
		exit(EXIT_SUCCESS);
	} else if (p < 0) {
		edie("fork");
	}

	if (recv(s, buf, sizeof(buf), 0) != sizeof(buf))
		edie("recv");

	c = 0;

	if (core == 0) {
		unsigned int i;
		
		gemaphore_p(&shared->gema);

		if (signal(SIGALRM, sighandler) == SIG_ERR)
			edie("signal(SIGALRM)");
			
		alarm(the_args.time);
		start = usec();

		for (i = 0; i < NPMC; i++)
			pmc_start[i] = read_pmc(i);

		mtrace_enable_set(1, TESTNAME);
		shared->run = 1;
	} else {
		gemaphore_v(&shared->gema);
		while (shared->run == 0)
			__asm __volatile ("pause");
	}
	while (shared->run) {
		int r;
		
		r = sendto(s, buf, sizeof(buf), 0, 
			   (struct sockaddr*)&dst, sizeof(dst));
		if (r < 0)
			edie("sendto");
		else if (r == 0)
			break;

		r = recv(s, buf, sizeof(buf), 0);
		if (r < 0)
			edie("recv");
		else if (r == 0)
			break;

		shared->count[core].v++;
	}

	sendto(s, buf, sizeof(buf), 0, 
	       (struct sockaddr*)&dst, sizeof(dst));

	close(c);
	close(s);
}

static void *worker(void *x)
{
	test((int)(intptr_t)x);
	return NULL;
}

static void initshared(void)
{
	shared = mmap(0, sizeof(*shared), PROT_READ|PROT_WRITE, 
		      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared == MAP_FAILED)
		die("mmap failed");
	gemaphore_init(&shared->gema, the_args.ncores - 1);
}

int main(int ac, char **av)
{
	unsigned int i;

	argv_parse(ac, av, &the_args, valid_args);

	initshared();

	for (i = 1; i < the_args.ncores; i++) {
		if (the_args.use_threads) {
			pthread_t th;
			if (pthread_create(&th, NULL, worker, (void *)(intptr_t)i) < 0)
				edie("pthread_create");
		} else {
			pid_t p;
			p = fork();
			if (p < 0)
				edie("fork");
			else if (p == 0) {
				test(i);
				return 0;
			}
		}
	}

	test(0);
	return 0;
}
