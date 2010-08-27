#include "lib/cpumap.h"
#include "lib/bench.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

int lcpu_to_pcpu[JOS_NCPU];

struct processor {
    int processor;	// logical cpu id
    int physical_id;	// physical package id of the processor
    int siblings;	// number of processors in a physical processor, including
			// both hardware threads and cores
    int core_id;	// core id in a physical processor
    int cpu_cores;	// number of cores in a physical package
};

struct cpu_core {
    struct processor **processors;
    int navail;
};

struct package {
    struct cpu_core *cores;		// number of cores in a physical cpu
};

void
cpumap_init()
{
#define RR
#ifdef RR
    FILE *fd = fopen("/proc/cpuinfo", "r");
    assert(fd != NULL);
    struct processor processors[JOS_NCPU];
    memset(processors, 0, sizeof(processors));
    int curproc = -1;
    char *line = NULL;
    int max_physical_id = 0;
    int max_cpu_cores = 0;
    int max_siblings = 0;
    size_t len = 0;
    while (getline(&line, &len, fd) != EOF) {
        int procid;
	if (sscanf(line, "processor\t: %d", &procid) == 1) {
	    curproc++;
	    processors[curproc].processor = procid;
	    continue;
	}
	if (sscanf(line, "physical id\t: %d", &processors[curproc].physical_id) == 1) {
	    max_physical_id = max(max_physical_id, processors[curproc].physical_id);
	    continue;
	}
	if (sscanf(line, "siblings\t: %d", &processors[curproc].siblings) == 1) {
	    max_siblings = max(max_siblings, processors[curproc].siblings);
	    continue;
	}
	if (sscanf(line, "core id\t: %d", &processors[curproc].core_id) == 1) {
	    continue;
	}
	if (sscanf(line, "cpu cores\t: %d", &processors[curproc].cpu_cores) == 1) {
	    max_cpu_cores = max(max_cpu_cores, processors[curproc].cpu_cores);
	    continue;
	}
    }
    fclose(fd);
    // construct the structrual representation of the processors
    int ncores_per_package = max_cpu_cores;
    int nsiblings_per_package = max_siblings;
    int npackages = max_physical_id + 1;
    int nsiblings_per_core = nsiblings_per_package / ncores_per_package;
    struct package *packs = (struct package *)malloc(sizeof(struct package) 
	* npackages);
    for (int i = 0; i < npackages; i++) {
	packs[i].cores = (struct cpu_core *)malloc(sizeof(struct cpu_core) 
	    * ncores_per_package);
	for (int j = 0; j < ncores_per_package; j++) {
	    packs[i].cores[j].processors = (struct processor **)calloc(
		nsiblings_per_core, sizeof(struct processor *));
	    packs[i].cores[j].navail = 0;
	}
    }

    for (int i = 0; i < JOS_NCPU; i++) {
	struct cpu_core *cpu_core = 
	    &packs[processors[i].physical_id].cores[processors[i].core_id];
	cpu_core->processors[cpu_core->navail++] = &processors[i];
    }
    // construct the cpumap in round robin manner
    int lcpuid = 0;
    for (int i = 0; i < nsiblings_per_core; i++) {
	for (int j = 0; j < ncores_per_package; j++) {
	    for (int k = 0; k < npackages; k++) {
		if (i >= packs[k].cores[j].navail)
		    continue;
	        lcpu_to_pcpu[lcpuid] = packs[k].cores[j].processors[i]->processor;
		if (debug)
	            fprintf(stderr, "%d->%d,", lcpuid, lcpu_to_pcpu[lcpuid]);
		lcpuid++;
	    }
	}
    }
    if (debug)
        fprintf(stderr, "\n");
#else
    // default cpumap provided by the system
    for (int i = 0; i < JOS_NCPU; i++)
	lcpu_to_pcpu[i] = i;
#endif
}
