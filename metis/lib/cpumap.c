#include "lib/cpumap.h"
#include "lib/bench.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#ifndef CPUMAP
int lcpu_to_pcpu[JOS_NCPU];
#else
int lcpu_to_pcpu[JOS_NCPU] = { CPUMAP };
#endif

void
cpumap_init()
{
#ifndef CPUMAP
    // default cpumap provided by the system
    for (int i = 0; i < JOS_NCPU; i++)
	lcpu_to_pcpu[i] = i;
#endif
}
