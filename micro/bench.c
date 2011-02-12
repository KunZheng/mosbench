#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#if defined (__SVR4) && defined (__sun)
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#endif

#include "bench.h"

void die(const char* errstr, ...) 
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

void edie(const char* errstr, ...) 
{
        va_list ap;

        va_start(ap, errstr);
        vfprintf(stderr, errstr, ap);
        va_end(ap);
        fprintf(stderr, ": %s\n", strerror(errno));
        exit(EXIT_FAILURE);
}

void setaffinity(int c)
{
#if defined (__SVR4) && defined (__sun)
	processorid_t obind;
	if (processor_bind(P_LWPID, P_MYID, c, &obind) < 0)
		edie("setaffinity, processor_bind failed");
#else
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(c, &cpuset);
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)
		edie("setaffinity: sched_setaffinity");
#endif
}

void memset_on(int core, void *s, int c, size_t n)
{
	cpu_set_t tmp;
	cpu_set_t org;

	if (sched_getaffinity(0, sizeof(org), &org) < 0)
		edie("memset_on: sched_getaffinity");

	CPU_ZERO(&tmp);
	CPU_SET(core, &tmp);
	if (sched_setaffinity(0, sizeof(tmp), &tmp) < 0)
		edie("memset_on: sched_setaffinity(tmp)");

	memset(s, c, n);

	if (sched_setaffinity(0, sizeof(org), &org) < 0)
		edie("memset_on: sched_setaffinity(org)");
}

uint64_t usec(void)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
