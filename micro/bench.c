#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "bench.h"

#define _STR(x) #x
#define STR(x) _STR(x)
#define MAX_PATH 256

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
	cpu_set_t cpuset;
	c = c % sysconf(_SC_NPROCESSORS_ONLN);
	CPU_ZERO(&cpuset);
	CPU_SET(c, &cpuset);
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)
		edie("setaffinity, sched_setaffinity failed");
}

uint64_t usec(void)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
