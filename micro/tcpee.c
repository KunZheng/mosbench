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
	.net_op = "tcp-ping",
};

static const char *valid_args[] = 
       { "time", "ncores", "net_op", NULL };

static void (*op_fn)(int arg);

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
	struct sockaddr_in addr;
	int c;

	c = socket(AF_INET, SOCK_STREAM, 0);
	if (c < 0)
		edie("socket");

	hostinfo = gethostbyname("127.0.0.1");

	memcpy(&addr.sin_addr.s_addr, hostinfo->h_addr, hostinfo->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

	if (connect(c, (struct sockaddr*)&addr, sizeof(addr)))
		edie("connect");

	for (;;) {
		char buf[1];
		int r;

		r = read(c, buf, 1);
		if (r < 0)
			edie("read");
		else if (r == 0)
			break;
		
		r = write(c, buf, 1);
		if (r < 0)
			edie("write");
		else if (r == 0)
			break;
	}
	close(c);
}

static void test(unsigned int core)
{
	struct sockaddr_in addr;
	socklen_t addr_sz;
	pid_t p;
	int one = 1;
	int s;
	int c;

	setaffinity(core % the_args.ncores);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		edie("socket");
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)))
		edie("setsockopt");
	
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(THE_PORT + core);
	addr.sin_family = AF_INET;

	if (bind(s,(struct sockaddr*)&addr, sizeof(addr)))
		edie("bind");
	if (listen(s, 5))
		edie("listen");

	p = fork();
	if (p == 0) {
		close(s);
		client(THE_PORT + core);
		exit(EXIT_SUCCESS);
	} else if (p < 0) {
		edie("fork");
	}

	addr_sz = sizeof(addr);
	c = accept(s, &addr, &addr_sz);
	if (c < 0)
		edie("accept");

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
		op_fn(c);
		shared->count[core].v++;
	}

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

static void tcp_ping_op(int c)
{
	char buf[1];
	int r;
	
	r = write(c, buf, 1);
	if (r < 0)
		edie("write");
	else if (r == 0)
		return;

	r = read(c, buf, 1);
	if (r < 0)
		edie("read");
	else if (r == 0)
		return;
}

static void set_op_fn(void)
{
	if (!strcmp(the_args.net_op, "tcp-ping"))
		op_fn = tcp_ping_op;
	else
		die("bad net_op: %s", the_args.net_op);
}

int main(int ac, char **av)
{
	unsigned int i;

	argv_parse(ac, av, &the_args, valid_args);

	set_op_fn();
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
