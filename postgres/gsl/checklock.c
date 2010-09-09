#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "modes-pg.h"
#include "gsl-parex.h"
#include "lc-int128-impl.h"

//////////////////////////////////////////////////////////////////
// Test case
//

#define CONFLICTS(a, b) (conflicts[a] & (1<<(b)))

lockCounts_t holderCounts;
bool observedSharing[LC_MAX_MODES];

void testLock(void)
{
        gslLockMode_t mode1 = PERPROC(curProc)->testMode1;
        bool mode1DontWait  = PERPROC(curProc)->testMode1DontWait;
        gslLockMode_t mode2 = PERPROC(curProc)->testMode2;
        bool mode2DontWait  = PERPROC(curProc)->testMode2DontWait;
        bool got1 = false, got2 = false;

        got1 = gslAcquire(&theLock, mode1, mode1DontWait, NULL);
        if (got1)
                holderCounts = lcAdd(holderCounts, lcActive(mode1));
        swtch();
        if (mode2 != -1) {
                got2 = gslAcquire(&theLock, mode2, mode2DontWait, NULL);
                if (got2)
                        holderCounts = lcAdd(holderCounts, lcActive(mode2));
                swtch();
        }

        // Check for conflicting held locks
        lockCounts_t otherCounts = lcSubtract(holderCounts, PERPROC(curProc)->perLock.holding);
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                if (lcGetActive(otherCounts, i)) {
                        if ((got1 && CONFLICTS(i, mode1)) || (got2 && CONFLICTS(i, mode2))) {
                                fail("Conflicting lock held");
                        } else if ((got1 && i == mode1) || (got2 && i == mode2)) {
                                observedSharing[i] = true;
                        }
                }
        }

        // I release mode1 first so that my tests for observed sharing
        // remain simple.  Otherwise, if mode1 overshadows mode2, then
        // it could prevent sharing of mode2, even if mode2 would
        // otherwise be sharable.  (This isn't entirely reliable, but
        // takes care of most cases)
        if (got1) {
                holderCounts = lcSubtract(holderCounts, lcActive(mode1));
                // XXX Check lastlocalhold and lasthold
                gslRelease(&theLock, mode1, NULL, NULL);
        }
        if (got2) {
                holderCounts = lcSubtract(holderCounts, lcActive(mode2));
                gslRelease(&theLock, mode2, NULL, NULL);
        }
}

//////////////////////////////////////////////////////////////////
// Test driver
//

void resetLock(void)
{
        memset(&theLock, 0, sizeof theLock);
        memset(&holderCounts, 0, sizeof holderCounts);
}

void doRun(void)
{
        // Print mode combination
        for (int i = 0; i < nProcs; ++i) {
                if (i != 0)
                        printf(" ");
                printf("%d%s/%d%s",
                       PERPROC(&procs[i])->testMode1,
                       PERPROC(&procs[i])->testMode1DontWait ? "(NB)" : "",
                       PERPROC(&procs[i])->testMode2,
                       PERPROC(&procs[i])->testMode2DontWait ? "(NB)" : "");
        }
        printf("\n");

        // Do I *expect* deadlock from this combination?  Ideally I
        // would ignore the particular bad interleavings, but this
        // will do.
        if (nProcs == 2) {
                gslLockMode_t
                        mode01 = PERPROC(&procs[0])->testMode1,
                        mode02 = PERPROC(&procs[0])->testMode2,
                        mode11 = PERPROC(&procs[1])->testMode1,
                        mode12 = PERPROC(&procs[1])->testMode2;
                if (mode02 != -1 && mode12 != -1) {
                        if (CONFLICTS(mode01, mode12) && CONFLICTS(mode02, mode11)) {
                                printf("SKIPPING\n");
                                return;
                        }
                }
        }

        // Reset expected sharing tracking
        memset(observedSharing, 0, sizeof observedSharing);

        // Reset counters
        memset(&counters, 0, sizeof counters);

        // Run every interleaving
        procRun();

        // Count number of threads for each lock mode
        int modeCounts[LC_MAX_MODES];
        memset(modeCounts, 0, sizeof modeCounts);
        for (int i = 0; i < nProcs; ++i) {
                modeCounts[PERPROC(&procs[i])->testMode1]++;
                if (PERPROC(&procs[i])->testMode2 != -1)
                        modeCounts[PERPROC(&procs[i])->testMode2]++;
        }

        // Check for expected sharing
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                if (modeCounts[i] > 1 && !CONFLICTS(i, i)) {
                        // Expected sharing to show up in some
                        // interleaving
                        if (!observedSharing[i])
                                printf("WARNING: No sharing observed for mode %d\n", i);
                }
        }

        // Print counters
        printf("acquireCalls: %d\t releaseCalls: %d\t "
               "wakeCalls: %d\t spuriousWakes: %d\n",
               counters.acquireCalls, counters.releaseCalls,
               counters.wakeCalls, counters.spuriousWakes);
}

void generateRun(int thread, int startMode, bool doUpgrades, bool doNonBlock)
{
        if (thread == nProcs) {
                doRun();
                return;
        }

        testPerProc_t *pls = PERPROC(&procs[thread]);

        for (int mode1 = startMode; mode1 < NUM_MODES; ++mode1) {
                pls->testMode1 = mode1;
                pls->testMode1DontWait = false;
                pls->testMode2 = -1;
                pls->testMode2DontWait = false;
                generateRun(thread + 1, mode1, doUpgrades, doNonBlock);
                if (doNonBlock) {
                        pls->testMode1DontWait = true;
                        generateRun(thread + 1, mode1, doUpgrades, doNonBlock);
                        pls->testMode1DontWait = false;
                }
                if (doUpgrades) {
                        for (int mode2 = 0; mode2 < NUM_MODES; ++mode2) {
                                if (mode1 == mode2)
                                        continue;
                                pls->testMode2 = mode2;
                                generateRun(thread + 1, mode1, doUpgrades, doNonBlock);
                                if (doNonBlock) {
                                        pls->testMode2DontWait = true;
                                        generateRun(thread + 1, mode1, doUpgrades, doNonBlock);
                                        pls->testMode2DontWait = false;
                                }
                        }
                }
        }
}

int main(int argc, char **argv)
{
        int threads = 2, switches = 7, doUpgrades = 0, doNonBlock = 0;
        //int threads = 2, switches = 7, doUpgrades = 0, doNonBlock = 1;
        //int threads = 3, switches = 5, doUpgrades = 0, doNonBlock = 1;
        //int threads = 4, switches = 3, doUpgrades = 0, doNonBlock = 0;

        procInit(resetLock, switches);

        assert(NUM_MODES <= LC_MAX_MODES);
        lcInitConflictTable(&conflictTable, conflicts, NUM_MODES);
        gslLockInit(&theLock, NULL);

        for (int i = 0; i < threads; ++i) {
                char name[2] = {i + 'A', 0};
                proc_t *proc = procAdd(name, testLock);
                testPerProc_t *perProc = calloc(1, sizeof *perProc);
                proc->user = perProc;
        }

        generateRun(0, 0, doUpgrades, doNonBlock);

        return 0;
}
