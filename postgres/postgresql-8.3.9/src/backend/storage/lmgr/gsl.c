/*-------------------------------------------------------------------------
 *
 * gsl.c
 *	  Postgres bindings of the GSL library.
 *
 *-------------------------------------------------------------------------
 */
#ifdef LOCK_SCALABLE

#include "postgres.h"
#include "storage/proc.h"
#include "storage/gsl.h"

#include "../../../../../gsl/lc-int128-impl.h"

conflictTable_t LockGSLConflicts;
gslPerProcLocal_t LockGSLPerProc;
HTAB *LockGSLPerLock;

#define GSL_CURPROC() MyProc
#define GSL_INC_COUNTER(x) do { } while (0)
#define GSL_PP() do { } while (0)

static inline conflictTable_t *GSL_CONFLICT_TABLE(gslLock_t *lock)
{
	return &LockGSLConflicts;
}

static inline gslPerProcShared_t *GSL_SHARED(PGPROC *proc)
{
	return &proc->gslPerProc;
}

static inline gslPerProcLocal_t *GSL_LOCAL(void)
{
	return &LockGSLPerProc;
}

static inline gslPerProcPerLock_t *GSL_PERLOCK(gslLock_t *lock)
{
	// XXX These might not get cleaned up in all cases
	// XXX Perhaps this should be created when we create the lock
	bool found;
	GSLPERLOCK *r =
		hash_search(LockGSLPerLock, (void*)&lock, HASH_ENTER, &found);
	if (!found) {
		MemSet(&r->pl, 0, sizeof r->pl);
	}
	return &r->pl;
}

static inline void GSL_WAKE(PGPROC *proc)
{
	ProcWakeup(proc, STATUS_OK);
}

static inline void GSL_MUTEX_ACQUIRE(LWLockId *m)
{
	LWLockAcquire(*m, LW_EXCLUSIVE);
}

static inline void GSL_MUTEX_RELEASE(LWLockId *m)
{
	LWLockRelease(*m);
}

#define GSL_MUTEX_ASSERT_LOCKED(m) do {} while (0)

#include "../../../../../gsl/gsl-impl.h"

bool gslHolding(gslLock_t *l)
{
	return lcNonZero(GSL_PERLOCK(l)->holding);
}

bool gslAnyHolding(gslLock_t *l)
{
	return lcNonZero(l->c);
}

#endif
