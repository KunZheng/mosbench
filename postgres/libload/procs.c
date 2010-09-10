#define _GNU_SOURCE // For MAP_ANONYMOUS

#include "libload.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/wait.h>

static double
floatTime(void)
{
        struct timeval tv;
        if (gettimeofday(&tv, NULL) == -1) {
                perror("gettimeofday failed");
                exit(1);
        }
        return (double)tv.tv_sec + (double)tv.tv_usec/1000000;
}

static void
onSigint(int signum)
{
        signal(SIGINT, SIG_DFL);
        if (sharedMem)
                requestShutdown();
}

void
createWorkerProcs(int num, void (*worker)(void))
{
        // Create the shared memory segment
        int shmemsize = sizeof *sharedMem + num * sizeof sharedMem->counts[0];
        sharedMem = mmap(NULL, shmemsize,
                         PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,
                         -1, 0);
        if (sharedMem == MAP_FAILED)
                epanic("mmap failed");
        memset((void*)sharedMem, 0, shmemsize);

        // Set up a signal handler to shut down cleanly on SIGINT
        signal(SIGINT, onSigint);

        // Create and run workers
        int pids[num];
        for (int i = 0; i < num; ++i) {
                pids[i] = fork();
                if (pids[i] == 0) {
                        // Set up death signal
                        if (prctl(PR_SET_PDEATHSIG, SIGINT) < 0)
                                epanic("prctl(PR_SET_PDEATHSIG) failed");
                        // Did the parent already die?
                        if (getppid() == 1)
                                kill(getpid(), SIGINT);
                        // Run worker
                        workerID = i;
                        worker();
                        requestShutdown();
                        return;
                }
                if (pids[i] == -1)
                        epanic("fork failed");
        }

        // Wait for all workers to start working
        for (int i = 0; i <= num; ++i) {
                fprintf(stderr, "\rSyncing workers... %d/%d", i, num);
                if (i < num)
                        while (sharedMem->counts[i] == 0 && !shutdownQueued())
                                // Sleep 100 ms
                                usleep(100000);
                if (shutdownQueued())
                        break;
        }
        fprintf(stderr, "\n");

        // Get initial counts
        int64_t counts[num];
        for (int i = 0; i < num; ++i)
                counts[i] = sharedMem->counts[i];

        int total = 0;
        int samples = 0;
        double start = floatTime();
        while (1) {
                // Do our best to sample once per second
                double now = floatTime();
                useconds_t usec = (start - now + samples + 1) * 1000000;
                if (usec <= 1000000)
                        usleep(usec);
                else
                        fprintf(stderr, "Not keeping up (usec %d)\n", usec);

                if (shutdownQueued())
                        break;

                // Read counts
                int64_t newCounts[num];
                for (int i = 0; i < num; ++i)
                        newCounts[i] = sharedMem->counts[i];

                // Diff against previous counts
                int sampleTotal = 0;
                for (int i = 0; i < num; ++i) {
                        sampleTotal += newCounts[i] - counts[i];
                }
                memcpy(counts, newCounts, sizeof counts);

                // Generate sample statistics
                total += sampleTotal;
                samples++;

                if (printf("%f %d (avg %d)\n", floatTime(), sampleTotal,
                           total/(samples*num)) <= 0
                    || fflush(stdout) != 0) {
                        // Nobody's listening any more.  Shut down.
                        break;
                }
        }

        // Wait for workers
        requestShutdown();
        for (int i = 0; i <= num; ++i) {
                fprintf(stderr, "\rWaiting for workers... %d/%d", i, num);
                if (i < num)
                        while (wait(NULL) < 0 && errno == EINTR);
        }
        fprintf(stderr, "\n");
        exit(0);
}
