/*
 * Create tasks, mmap a file, access different pages in the file
 */

#define TESTNAME "pagecache"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include "bench.h"
#include "support/mtrace.h"
#include "forp.h"
#include "gemaphore.h"

#define MAX_PROC 256

static unsigned int ncores;

static char  the_file[128];
static off_t the_file_sz;

static uint64_t core_byte_count[MAX_PROC];
static uint64_t core_file_offset[MAX_PROC];

static struct {
	union __attribute__((__aligned__(64))){
		struct {
			volatile uint64_t v;
			volatile uint64_t usecs;
		};
		char pad[64];
	} count[MAX_PROC];

	volatile int run;
	struct gemaphore gema;
} *shared;

static void test(unsigned int proc)
{
	uint64_t start;
	size_t pgsize;
	void *file;
	void *end;
	int fd;

	setaffinity(proc % ncores);

	fd = open(the_file, O_RDWR);
	if (fd < 0)
		edie("open %s", the_file);
	file = mmap(NULL, the_file_sz, PROT_READ|PROT_WRITE, 
		    MAP_PRIVATE, fd, 0);
	if (file == MAP_FAILED)
		edie("mmap %s", the_file);
	file += core_file_offset[proc];
	end = file + core_byte_count[proc];

	pgsize = get_page_size();

	if (proc == 0) {
		gemaphore_p(&shared->gema);
		forp_reset();
		mtrace_enable_set(1, TESTNAME);
		shared->run = 1;
	} else {
		gemaphore_v(&shared->gema);
		while (shared->run == 0)
			nop_pause();
	}

	start = usec();
	while (file != end) {
		volatile char *load = file;
		char val;

		val = *load;

		file += pgsize;
		shared->count[proc].v++;
	}
	shared->count[proc].usecs = usec() - start;
}

static void initshared(void)
{
	shared = mmap(0, sizeof(*shared), PROT_READ|PROT_WRITE, 
		      MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (shared == MAP_FAILED)
		die("mmap failed");
	gemaphore_init(&shared->gema, ncores - 1);
}

static void xwrite(int fd, const void *buf, size_t count)
{
	int cc;

	while (count) {
		cc = write(fd, buf, count);
		if (cc < 0)
			edie("write");
		else if (cc == 0)
			die("write returned 0");
		buf += cc;
		count -= cc;
	}
}

static void waitup(void)
{
	struct mtrace_appdata_entry entry;
	unsigned int i;
	uint64_t tot;
	float rate;

	rate = 0.0;
	tot = 0;
	for (i = 0; i < ncores; i++) {
		float r;

		while (!shared->count[i].usecs)
			nop_pause();
		
		r = (float)shared->count[i].v / (float)shared->count[i].usecs;
		tot += shared->count[i].v;
		rate += r;
	}
	entry.u64 = tot;
	mtrace_appdata_register(&entry);
	mtrace_enable_set(0, TESTNAME);

	rate = rate * 1000000.0;
	printf("%f\n", rate);
}

static void initfile(void)
{
	unsigned int i;
	uint64_t k;
	char *buf;
	int fd;

	fd = creat(the_file, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd < 0)
		edie("creat");

	buf = malloc(get_page_size());
	memset(buf, 0, get_page_size());

	for (i = 0; i < ncores; i++) {
		setaffinity(i);
		/*
		 * Assumes core_file_offset[0] starts at 0 and increases
		 * with each core.
		 */
		for (k = 0; k < core_byte_count[i]; k += get_page_size())
			xwrite(fd, buf, get_page_size());
	}
	free(buf);
	close(fd);
}

int main(int ac, char **av)
{
	unsigned int i;
	off_t npages;

	if (ac < 4)
		die("usage: %s num-cores base-filename file-size", av[0]);

	setaffinity(0);

	ncores = atoi(av[1]);
	snprintf(the_file, sizeof(the_file), "%s/pagecache", av[2]);
	the_file_sz = strtoll(av[3], NULL, 10);

	initshared();

	npages = (the_file_sz + (get_page_size() - 1)) / get_page_size();
	for (i = 0; i < npages; i++)
		core_byte_count[i % ncores] += get_page_size();
	for (i = 1; i < ncores; i++)
		core_file_offset[i] = core_file_offset[i - 1] + core_byte_count[i - 1];
	initfile();

	for (i = 1; i < ncores; i++) {
		pid_t p;

		p = fork();
		if (p < 0)
			edie("fork");
		else if (p == 0) {
			test(i);
			return 0;
		}
	}

	test(0);
	waitup();
	return 0;
}
