#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define __notret__ __attribute__((noreturn))
#define __align__ __attribute__((aligned(CACHE_BYTES)))

void __notret__ die(const char* errstr, ...);
void __notret__ edie(const char* errstr, ...);

void setaffinity(int c);

uint64_t usec(void);

static inline uint64_t read_tsc(void)
{
	uint32_t a, d;
	__asm __volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((uint64_t) a) | (((uint64_t) d) << 32);
}

static inline void * xmalloc(unsigned int sz)
{
	size_t s;
	void *r;
	
	s = ((sz - 1) + 64) & ~63;
	if (posix_memalign(&r, 64, s))
		edie("posix_memalign");
	memset(r, 0, sz);
	return r;
}

static inline uint32_t rnd(uint32_t *seed)
{
	*seed = *seed * 1103515245 + 12345;
	return *seed & 0x7fffffff;
}

static inline void nop_pause(void)
{
	__asm __volatile("pause");
}

#if ENABLE_PMC
static inline uint64_t read_pmc(uint32_t ecx)
{
	uint32_t a, d;
	__asm __volatile("rdpmc" : "=a" (a), "=d" (d) : "c" (ecx));
	return ((uint64_t) a) | (((uint64_t) d) << 32);
}
#else /* !ENABLE_PMC */
static inline uint64_t read_pmc(uint32_t ecx)
{
	return 0;
}
#endif
