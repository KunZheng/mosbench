#include <assert.h>

#include "gsl-parex.h"

gslLock_t theLock;

conflictTable_t conflictTable;

#define GSL_CURPROC() curProc
#define GSL_INC_COUNTER(c) do { ++counters.c; } while (0)
#define GSL_PP swtch

static inline conflictTable_t *GSL_CONFLICT_TABLE(gslLock_t *lock)
{
        assert(&theLock == lock);
        return &conflictTable;
}

static inline gslPerProcShared_t *GSL_SHARED(proc_t *proc)
{
        return &PERPROC(proc)->shared;
}

static inline gslPerProcLocal_t *GSL_LOCAL(void)
{
        return &PERPROC(curProc)->local;
}

static inline gslPerProcPerLock_t *GSL_PERLOCK(gslLock_t *lock)
{
        assert(&theLock == lock);
        return &PERPROC(curProc)->perLock;
}

static inline void GSL_ACQUIRE_SLEEP(mutex_t *m, void *sleepArg)
{
        mutexRelease(m);
        procSleep();
}

#define GSL_WAKE procWake
#define GSL_MUTEX_ACQUIRE mutexLock
#define GSL_MUTEX_RELEASE mutexRelease
#define GSL_MUTEX_ASSERT_LOCKED(m) do { assert((m)->value); } while (0)

#include "lc-int128-impl.h"
#include "gsl-impl.h"
