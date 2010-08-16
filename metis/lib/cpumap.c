#include "lib/cpumap.h"
#include <stdlib.h>
#include <stdio.h>

int lcpu_to_pcpu[JOS_NCPU];

void
cpumap_init()
{
#define RR
#ifdef RR
    fprintf(stderr, "Warning: we assumes the system is equipped with eight six-core processors.\n");
    for (int i = 0; i < 8; i ++) {
	for (int j = 0; j < 6; j++)
	    lcpu_to_pcpu[j * 8 + i] = i * 6 + j;
    }
    for (int i = 0; i < 6; i ++) {
	for (int j = 0; j < 8; j++) {
	    printf("%d,", lcpu_to_pcpu[i * 8 + j]);
	}
	printf("\n");
    }
#else
    for (int i = 0; i < JOS_NCPU; i++)
	lcpu_to_pcpu[i] = i;
#endif
}
