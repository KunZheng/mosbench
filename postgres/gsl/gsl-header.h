#ifndef GSL_H
#define GSL_H

/*
 * The appropriate lock counter header must be included before this.
 *
 * Required definitions:
 *
 * gslProc_t - An opaque type representing a process.  Always used via
 * a pointer.
 *
 * gslMutex_t - A mutex type.
 */

typedef struct gslPerProcShared_t
{
        // Link for lock_t.waiter list.  NULL if not on a waiters
        // list.  Protected by the queue lock.
        gslProc_t *lockNext;
        // When on a lock waiter list, this records the desired mode.
        // This can be read by anyone, but is only written by the
        // local proc.
        gslLockMode_t wantMode;
        // When on a lock waiter list, whether this is an upgrade
        // request (and thus unfair)
        bool isUpgrade;
        // When on a lock waiter list, the holding array for this
        // process on this lock (see gslPerProcPerLock_t).
        lockCounts_t waitHolding;
} gslPerProcShared_t;

typedef struct gslPerProcLocal_t
{
        // The lock I am currently waiting on.  This is used to
        // implement lock canceling.  If this is non-null, then
        // GSL_SHARED(curProc)->wantMode indicates the mode it is
        // being acquired in.
        struct gslLock_t *awaitedLock;
} gslPerProcLocal_t;

typedef struct gslPerProcPerLock_t
{
        // Which lock modes this process holds on this lock.  This is
        // only ever accessed locally.
        lockCounts_t holding;
} gslPerProcPerLock_t;

typedef struct gslLock_t
{
        lockCounts_t c;
        // The queue lock protects the wait queue and the waitCounts
        // array.
        gslMutex_t queueLock;
        // The queue of processes blocked on this lock.  This queue is
        // logically split into two parts.  The beginning of the queue
        // consists of unfair upgrade requests, which may be satisfied
        // in any order.  The rest of the queue consists of fair
        // regular acquire requests, which must be satisfied in order.
        // Thus, upgrade requests are always prepended to the queue,
        // while regular acquire requests are appended.  The two
        // regions can be distinguished by the 'isUpgrade' flag in the
        // proc.
        //
        // Postgres is somewhat more clever about this.  For unfair
        // requests, it carefully chooses where to put them in the
        // queue so they don't conflict.  This lets it detect some
        // deadlocks early, but isn't otherwise clearly advantageous.
        gslProc_t *waiter;
        gslProc_t **waiterTail;
        int waitCounts[LC_MAX_MODES];
} gslLock_t;

void gslLockInit(gslLock_t *lock, gslMutex_t **queueLock);
bool gslAcquire(gslLock_t *lock, gslLockMode_t mode, bool dontWait, void *sleepArg);
void gslRelease(gslLock_t *lock, gslLockMode_t mode, bool *lastLocalHold, bool *lastHold);
bool gslCancel(gslLock_t *lock, bool *lastLocalHold);

#endif
