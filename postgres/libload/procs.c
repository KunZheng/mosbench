#define _GNU_SOURCE // For MAP_ANONYMOUS

#include "libload.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/wait.h>

static int numWorkers;
static int64_t *initialCounts;
static int64_t prevTotal;

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

static void
resetCounts(int x)
{
        if (x != 0)
                printf("[SIG] Resetting counts\n");
        for (int i = 0; i < numWorkers; ++i)
                initialCounts[i] = sharedMem->counts[i];
        prevTotal = 0;
}

static void
printCount(int x)
{
        // Read counts
        int64_t newCounts[numWorkers];
        for (int i = 0; i < numWorkers; ++i)
                newCounts[i] = sharedMem->counts[i];

        // Diff against initial counts
        int64_t total = 0;
        for (int i = 0; i < numWorkers; ++i) {
                total += newCounts[i] - initialCounts[i];
        }

        if (x != 0)
                printf("[SIG] ");
        if (printf("%"PRIi64" total queries, %"PRIi64" delta\n", total,
                   total - prevTotal) <= 0
            || fflush(stdout) != 0) {
                // Nobody's listening any more.  Shut down.
                requestShutdown();
        }

        prevTotal = total;
}

void
createWorkerProcs(int num, void (*worker)(void))
{
        // Set up global state
        numWorkers = num;
        initialCounts = malloc(num * sizeof *initialCounts);
        if (!initialCounts)
                panic("failed to allocate counts");


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

        // Flush stdout so our children don't inherit any buffered data
        fflush(stdout);

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
        resetCounts(0);

        // Install signal handlers.  We have to do this after we get
        // the initial counts so we don't race.  Also, by doing this
        // after the workers are sync'd, if we get a signal before
        // we're ready, we'll exit.
        signal(SIGUSR1, resetCounts);
        signal(SIGUSR2, printCount);

        int samples = 0;
        double start = floatTime();
        while (1) {
                // Do our best to sample once per second
                double target = start + samples + 1;
                double now = floatTime();
                if (target < now)
                        fprintf(stderr, "Not keeping up (%g sec behind)\n", target - now);
                else
                        usleep((target - now) * 1000000);

                if (shutdownQueued())
                        break;

                printCount(0);
                ++samples;
        }

        // Wait for workers
        requestShutdown();
        for (int i = 0; i <= num; ++i) {
                fprintf(stderr, "\rWaiting for worker shutdown... %d/%d", i, num);
                if (i < num)
                        while (wait(NULL) < 0 && errno == EINTR);
        }
        fprintf(stderr, "\n");
        exit(0);
}
