#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "config.h"

#if defined (__SVR4) && defined (__sun)
#include <sys/mman.h>
#include <sys/inttypes.h>
#include <unistd.h>
#else
#include <sys/user.h>
#include <inttypes.h>
#endif

#define __noret__ __attribute__((noreturn))
#define __align__ __attribute__((aligned(CACHE_BYTES)))

void __noret__ die(const char* errstr, ...);
void __noret__ edie(const char* errstr, ...);

void setaffinity(int c);

uint64_t usec(void);

static inline uint64_t read_tsc(void)
{
	uint32_t a, d;
	__asm __volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((uint64_t) a) | (((uint64_t) d) << 32);
}

static inline unsigned int get_page_size(void)
{
#if defined (__SVR4) && defined (__sun)
	return sysconf(_SC_PAGE_SIZE);
#else
	return PAGE_SIZE;
#endif
}

static inline void * xmalloc(unsigned int sz)
{
	size_t s;
	void *r;
	
	s = ((sz - 1) + CACHE_BYTES) & ~(CACHE_BYTES - 1);
#if defined (__SVR4) && defined (__sun)
	r = memalign(s, CACHE_BYTES);
#else
	if (posix_memalign(&r, CACHE_BYTES, s))
		edie("posix_memalign");
#endif
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

static inline void rep_nop(void)
{
	__asm __volatile("rep; nop" ::: "memory");
}

static inline void cpu_relax(void)
{
	rep_nop();
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
