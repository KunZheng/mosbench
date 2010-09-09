#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "parex.h"

// I could substantially prune the search tree if I knew for each
// segment what shared state had been read or written.  For example,
// there could be multiple forms of PP/sched, one for each reason you
// might have a preemption point, that take the addresses affected by
// the preemption point.  For example, PP_READ(void* addr, int size),
// PP_WRITE(void *addr, int size), PP_READWRITE(void* addr, int
// size).
//
// For example, for the following tree:
//          PP
//         A  B
//  PP_READ    PP_READ
// After exploring the A subtree, then stepping the B subtree, I know
// I didn't have to explore any more of the B subtree.
//
// I could even require global reads and writes to be written as PP
// calls that would automatically DTRT, especially with respect to
// preemption points *within* non-atomic reads or writes.  This has
// the added benefit that parex will know what the operation is before
// it does it, so it can record the operation and perform a
// preemption, thus making it easy to determine which schedules are
// distinguishable *without* having to replay them (a huge win) and
// also making debug traces nicer.
//
// A more natural way to explore the search space would be to keep a
// stack of unexplored paths to back track to (more akin to how
// generalized tree search algorithms work).  I'd keep the part of the
// search tree that I hadn't explored yet and that I was currently
// working on.  Then I could store the explored preemption reasons on
// this tree so I could do the above pruning.  This would also make it
// easy to explore all 0 preemptions, then all 1 preemptions, etc, by
// prioritizing the queue.

proc_t *curProc;
proc_t *procs;
int nProcs;
static int maxProcs;

static pth_t scheduler;
static void (*resetFn)(void);
static int switches;
static int maxSwitches;

#define MAX_DEPTH 100
static pth_t searcher;
static int initPath[MAX_DEPTH];
static proc_t *path[MAX_DEPTH];
static const char *pathNames[MAX_DEPTH];
static int pathLength;
static bool backtracked;

//////////////////////////////////////////////////////////////////
// Setup
//

void procInit(void (*resetFn_)(void), int maxSwitches_)
{
        resetFn = resetFn_;
        maxSwitches = maxSwitches_;

        maxProcs = 4;
        procs = malloc(maxProcs * sizeof *procs);
        assert(procs);

        assert(pth_init());
}

proc_t *procAdd(const char *name, void (*entry)(void))
{
        if (nProcs == maxProcs) {
                maxProcs *= 2;
                procs = realloc(procs, maxProcs * sizeof *procs);
                assert(procs);
        }

        procs[nProcs].name = strdup(name);
        procs[nProcs].entry = entry;
        return &procs[nProcs++];
}

//////////////////////////////////////////////////////////////////
// Searcher
//

void procPrintPath(void)
{
        for (int i = 0; i < pathLength; ++i) {
                if (i != 0)
                        printf(" -> ");
                printf("%s (%s)", path[i]->name, pathNames[i]);
        }
        printf("\n");
}

static void procSearchRec(int depth)
{
        bool anyRunnable = false;
        int init = initPath[depth];
        initPath[depth] = 0;

        if (depth == MAX_DEPTH)
                return;

        // Snapshot process states
        enum procState_t states[nProcs];
        for (int i = 0; i < nProcs; ++i)
                states[i] = procs[i].state;

        // Are all procs done?
        for (int i = 0; i < nProcs; ++i) {
                if (states[i] != DONE)
                        goto nonterminal;
        }
//        procPrintPath();
        return;
nonterminal:

        // Walk runnable procs
        for (int i = init; i < nProcs; ++i) {
                if (states[i] == RUNNABLE) {
                        anyRunnable = true;
                        // Schedule this
                        path[depth] = &procs[i];
                        pathNames[depth] = "";
                        pathLength = depth + 1;
                        pth_yield(scheduler);
                        procSearchRec(depth + 1);
                        backtracked = true;
                }
        }

        if (!anyRunnable) {
                printf("ERROR: Deadlock\n");
                procPrintPath();
                abort();
        }
}

static void *procSearch(void *opaque)
{
        procSearchRec(0);
        // Indicate that we've run out of tree
        pathLength = 0;
        pth_yield(scheduler);
}

//////////////////////////////////////////////////////////////////
// Scheduler
//

static void procYield(proc_t *proc)
{
        if (proc != curProc)
                switches++;
        curProc = proc;
        assert(curProc->state == RUNNABLE);
//        printf("Stepping %s\n", curProc->name);
        pth_yield(curProc->pth);
}

static void *procEntry(void *opaque)
{
        proc_t *proc = (proc_t*)opaque;
        proc->entry();
        proc->state = DONE;
        _swtch("end");
        assert(0);
        return NULL;
}

static void procReset(void)
{
        for (int i = 0; i < nProcs; ++i) {
                // Save preserved state
                const char *name = procs[i].name;
                void (*entry)(void) = procs[i].entry;
                void *user = procs[i].user;
                // Stop running thread if there is one
                if (procs[i].state != UNINITIALIZED)
                        pth_abort(procs[i].pth);
                // Reset proc state
                memset(&procs[i], 0, sizeof procs[i]);
                // Restore preserved state
                procs[i].name = name;
                procs[i].entry = entry;
                procs[i].user = user;
                // Setup the proc
                procs[i].state = RUNNABLE;
                procs[i].pth = pth_spawn(PTH_ATTR_DEFAULT, procEntry, &procs[i]);
        }

        switches = 0;
        curProc = NULL;
        resetFn();
}

void procRun(void)
{
        scheduler = pth_self();
        searcher = pth_spawn(PTH_ATTR_DEFAULT, procSearch, NULL);

        procReset();
        while (true) {
                pth_yield(searcher);

                if (pathLength == 0)
                        // The search space has been exhausted
                        break;

                if (backtracked) {
                        backtracked = false;
                        // Start at the top of the tree and replay
                        procReset();
//                        printf("Replaying: ");
//                        procPrintPath();
                        for (int i = 0; i < pathLength; ++i)
                                procYield(path[i]);
                } else {
                        // Take one step forward
                        procYield(path[pathLength - 1]);
                }
        }
}

void _swtch(const char *name)
{
        if (curProc->state == RUNNABLE && switches > maxSwitches)
                return;

        pathNames[pathLength - 1] = name;
        pth_yield(scheduler);
}

void fail(const char *str)
{
        printf("FAIL: %s\n", str);
        procPrintPath();
        abort();
}

//////////////////////////////////////////////////////////////////
// Sleep/wake
//

void procSleep(void)
{
        ++curProc->sleepCount;
        assert(curProc->sleepCount <= 1);
        assert(curProc->sleepCount >= 0);

        if (curProc->sleepCount == 0)
                return;

        curProc->state = STOPPED;
        _swtch("sleep");
}

void procWake(proc_t *p)
{
        --p->sleepCount;
        assert(p->sleepCount <= 0);
        assert(p->sleepCount >= -1);

        if (p->sleepCount == 0) {
                p->state = RUNNABLE;
                _swtch("wake other");
        }
}

//////////////////////////////////////////////////////////////////
// Mutexes
//

void mutexLock(mutex_t *m)
{
        if (!m->value) {
                m->value = true;
                _swtch("mutex acquire");        /* Necessary? */
                return;
        }
        if (!m->waiter)
                m->waiterTail = &m->waiter;
        *m->waiterTail = curProc;
        m->waiterTail = &curProc->mutexNext;
        curProc->mutexNext = NULL;
        curProc->state = STOPPED;
        _swtch("mutex wait");
        assert(m->value);
}

void mutexRelease(mutex_t *m)
{
        proc_t *waiter = m->waiter;
        if (!waiter) {
                m->value = false;
                _swtch("mutex release");
                return;
        }

        m->waiter = waiter->mutexNext;
        waiter->state = RUNNABLE;
        waiter->mutexNext = NULL;
        _swtch("mutex release/wake");
}
