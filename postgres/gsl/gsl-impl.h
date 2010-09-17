#include <string.h>

#define GSL_PARANOID

/*
 * The appropriate lock counter implementation must be included before
 * this.
 *
 * Required definitions:
 *
 * gslProc_t *GSL_CURPROC(void) - Return the current process.
 *
 * void GSL_INC_COUNTER(token counter) - Increment the given
 * statistics counter.  Can be a no-op.
 *
 * void GSL_PP(void) - Preemption point.  Can be a no-op.
 *
 * conflictTable_t *GSL_CONFLICT_TABLE(gslLock_t *lock) - Return the
 * conflict table for a given lock.
 *
 * gslPerProcShared_t *GSL_SHARED(gslProc_t *) - Return the shared
 * per-process state of the given proc.
 *
 * gslPerProcLocal_t *GSL_LOCAL(void) - Return the local per-process
 * state of the current process.
 *
 * gslPerProcPerLock_t *GSL_PERLOCK(gslLock_t *lock) - Return the
 * per-lock state of the given lock for this process.
 *
 * void GSL_ACQUIRE_SLEEP(gslMutex_t *, void *) - Release the given
 * mutex and put the current process to sleep.  If a wake already
 * happened, just release the mutex and return.  The mutex protects
 * the lock being acquired, so this gives the sleep function a chance
 * to manipulate sleep-related structures atomically with respect to
 * other holders of the lock.  The void * argument is the sleepArg
 * passed to gslAcquire.  This function is allowed to abort without
 * returning (for example, using longjmp).  This will not break shared
 * state, but the local state will be inconsistent until gslCancel is
 * called on the lock that was being acquired.  If gslCancel gets
 * called from within the sleep function itself, the sleep function
 * must *not* return to its caller.
 *
 * void GSL_WAKE(gslProc_t *) - Wake the given process.
 *
 * void GSL_MUTEX_ACQUIRE(gslMutex_t *) - Acquire a mutex.
 *
 * void GSL_MUTEX_RELEASE(gslMutex_t *) - Release a mutex.
 *
 * void GSL_MUTEX_ASSERT_LOCKED(gslMutex_t *) - Assert that a mutex is
 * locked.  This can be a no-op.
 */

/**
 * Initialize the given GSL lock.  It will not be held by any
 * waiters.  If the queue lock needs to be initialized (other than
 * zero'ing it out), pass queueLock and initialize the returned
 * gslMutex_t.
 */
void gslLockInit(gslLock_t *lock, gslMutex_t **queueLock)
{
        memset(lock, 0, sizeof *lock);
        if (queueLock)
                *queueLock = &lock->queueLock;
}

static void wake(gslLock_t *lock)
{
        GSL_MUTEX_ASSERT_LOCKED(&lock->queueLock);
        GSL_INC_COUNTER(wakeCalls);

        // Assert waitCounts is right
#ifdef GSL_PARANOID
        int expectWaitCounts[LC_MAX_MODES];
        memset(expectWaitCounts, 0, sizeof expectWaitCounts);
        for (gslProc_t *waiter = lock->waiter; waiter;
             waiter = GSL_SHARED(waiter)->lockNext) {
                ++expectWaitCounts[GSL_SHARED(waiter)->wantMode];
        }
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                assert(lock->waitCounts[i] == expectWaitCounts[i]);
        }
#endif

        // Walk the queue and process all unfair upgrade requests and
        // as many fair requests as possible.
        bool anyWoken = false;
        gslProc_t **prevLink = &lock->waiter;
        for (gslProc_t *proc = *prevLink; proc; proc = *prevLink) {
                assert(proc == *prevLink);

                gslLockMode_t wantMode = GSL_SHARED(proc)->wantMode;
                lockCounts_t procCounts = GSL_SHARED(proc)->waitHolding;
                lockCounts_t conflictMask =
                        lcConflictMask(GSL_CONFLICT_TABLE(lock),
                                       wantMode, true, false);
                lockCounts_t increment = lcActive(wantMode);
                lockCounts_t waitMask = lcMaskSet();
                if (lock->waitCounts[wantMode] == 1)
                        waitMask = lcSetWait(waitMask, wantMode, false);

                // Atomically check for conflicts with proc's desired
                // mode and, if there are no conflicts, increment its
                // active count and, if applicable, clear the wait bit
                // on its mode.
        retry:;
                lockCounts_t preCounts = lcGet(&lock->c);
                lockCounts_t otherCounts = lcSubtract(preCounts, procCounts);
                if (lcNonZero(lcAnd(otherCounts, conflictMask))) {
                        if (GSL_SHARED(proc)->isUpgrade) {
                                // Unfair.  Just move on to the next
                                // waiter.  Point prevLink at this
                                // element so the loop expression will
                                // follow it to the next proc.
                                prevLink = &GSL_SHARED(proc)->lockNext;
                                continue;
                        } else {
                                // Fair.  We can't even consider later
                                // waiters.
                                break;
                        }
                }

                // We can grant this lock.  Increment the mode's
                // active count.
                lockCounts_t counts = lcAdd(preCounts, increment);
                // If this was the last waiter for this mode, clear
                // the mode's wait bit.
                counts = lcAnd(counts, waitMask);
                if (!lcSet(&lock->c, preCounts, counts))
                        goto retry;
                GSL_PP();

                // Wake up the process we just granted the lock to.
                GSL_WAKE(proc);
                anyWoken = true;

                // Fix wait count
                --lock->waitCounts[wantMode];

                // Pull this proc out of the waiter list.  We update
                // *prevLink, but leave prevLink pointing at the
                // previous link, since the loop expression will
                // follow the new *prevLink to get to the next
                // element.
                *prevLink = GSL_SHARED(proc)->lockNext;
                GSL_SHARED(proc)->lockNext = NULL;
        }

        if (!anyWoken)
                GSL_INC_COUNTER(spuriousWakes);
}

bool gslAcquire(gslLock_t *lock, gslLockMode_t mode, bool dontWait,
                void *sleepArg)
{
        GSL_INC_COUNTER(acquireCalls);

        assert(!GSL_LOCAL()->awaitedLock);

        // I want to change the lock counts by incrementing the active
        // count for the requested mode
        lockCounts_t increment = lcActive(mode);

        // Subtract out modes that I hold, since I can't conflict with
        // myself
        lockCounts_t *holding = &GSL_PERLOCK(lock)->holding;
        lockCounts_t procCounts = *holding;
        assert(!lcGetActive(procCounts, mode));

        // If I already hold this lock in some mode, then this
        // acquisition is *not* fair.  I can move ahead of waiters.
        // This is to be consistent with Postgres lock policy (and to
        // reduce the chance of deadlock).
        bool holdLock = lcNonZero(procCounts);

        // Construct a conflict mask for the requested mode
        lockCounts_t conflictMask =
                lcConflictMask(GSL_CONFLICT_TABLE(lock),
                               mode, true, !holdLock);

        if (dontWait) {
                // Atomically get mode counts, check for conflicts,
                // and, if there are no conflicts, increment my mode
        retry1:;
                lockCounts_t preCounts = lcGet(&lock->c);
                // Check for conflicts
                lockCounts_t conflicts = lcAnd(lcSubtract(preCounts, procCounts),
                                               conflictMask);
                // We hint GCC that the non-conflicting branch is more
                // likely not because this is necessarily the case,
                // but because we need the atomic retry loop to be as
                // short as possible, while we don't care about the
                // other branch.
                if (__builtin_expect(lcZero(conflicts), 1)) {
                        // There are no conflicts, so increment my
                        // mode and update the lock counts
                        if (!lcSet(&lock->c, preCounts, lcAdd(preCounts, increment)))
                                goto retry1;
                        GSL_PP();

                        // Got the lock
                        goto acquired;
                } else {
                        // There are conflicts, so we can't acquire
                        // the lock.
                        return false;
                }
        }

        // Atomically get all mode counts and increment the requested
        // mode count
        //
        // XXX What if I just check for conflicts atomically?  Then I
        // don't have transient active counts, though the atomic loop
        // is longer and will contain a branch (though I could hint
        // the branch).
retry:;
        lockCounts_t preCounts = lcGet(&lock->c);
        if (!lcSet(&lock->c, preCounts, lcAdd(preCounts, increment)))
                goto retry;
        GSL_PP();

        // Check for conflicts
        preCounts = lcSubtract(preCounts, procCounts);
        if (lcZero(lcAnd(preCounts, conflictMask))) {
                // All of the conflicting mode counts and wait bits are zero,
                // so nobody holds a conflicting lock and nobody's in the wait
                // queue for a conflicting lock, so I have successfully
                // acquired the lock.
                goto acquired;
        }

        // There's some conflicting lock mode that's either held or
        // somebody is waiting to hold, so I need to wait.
        GSL_MUTEX_ACQUIRE(&lock->queueLock);

        // Undo the change to the active count I made above and mark
        // that I'm waiting on the requested mode.  I can do this
        // before I add myself to the wait queue because anything else
        // accessing the wait queue must hold the queue lock.  Doing
        // it early makes the window with the transient active count
        // shorter.
        // XXX Two parts need to be atomic?
retry2:
        preCounts = lcGet(&lock->c);
        lockCounts_t counts = lcSubtract(preCounts, increment);
        counts = lcOr(counts, lcSetWait(LC_ZERO, mode, true));
        if (!lcSet(&lock->c, preCounts, counts))
                goto retry2;
        GSL_PP();

        // Add myself to the wait queue
        if (holdLock) {
                // Add myself to the head and mark this as an upgrade,
                // so it won't be treated fairly by wake.
                if (!lock->waiter)
                        lock->waiterTail = &GSL_SHARED(GSL_CURPROC())->lockNext;
                GSL_SHARED(GSL_CURPROC())->lockNext = lock->waiter;
                lock->waiter = GSL_CURPROC();
                GSL_SHARED(GSL_CURPROC())->isUpgrade = true;
        } else {
                // Add myself to the tail and mark this as fair, so if
                // I can't get the lock, nobody after me in the queue
                // can either.
                if (!lock->waiter)
                        lock->waiterTail = &lock->waiter;
                *lock->waiterTail = GSL_CURPROC();
                lock->waiterTail = &GSL_SHARED(GSL_CURPROC())->lockNext;
                GSL_SHARED(GSL_CURPROC())->lockNext = NULL;
                GSL_SHARED(GSL_CURPROC())->isUpgrade = false;
        }
        ++lock->waitCounts[mode];
        GSL_SHARED(GSL_CURPROC())->wantMode = mode;
        GSL_SHARED(GSL_CURPROC())->waitHolding = procCounts;
        // (No need to GSL_PP, since the only other user of the wait
        // queue is protected by the queue lock)

        // The process that conflicted with my request may have
        // released the lock while I was busy, so try a wake.  We
        // could check the conflicting counts at this point since we
        // have them handy and save ourselves a useless call to wake
        // if any of them are non-zero, but that's just duplicating
        // the logic in wake.  It would save us an atomic read in the
        // short-circuit case, but require more computation
        // otherwise.  We've already acquire the queue lock, which is
        // the expensive part.
        wake(lock);
        GSL_PP();

        // Record what lock I'm waiting on, in case I get canceled in
        // my sleep.
        GSL_LOCAL()->awaitedLock = lock;

        // Sleep.  Once this returns, all of the conflicting locks
        // have been released and we've been granted our requested
        // mode.  NOTE: This is allowed to abort without returning
        // through this function!  Thus, we must not modify any shared
        // state after this point, and any local state we modify must
        // get patched up by gslCancel.
        GSL_ACQUIRE_SLEEP(&lock->queueLock, sleepArg);

        // Make sure I wasn't canceled during the sleep and then
        // returned to.  If this happens, gslCancel may have dabbled
        // in the local state.
        assert(GSL_LOCAL()->awaitedLock == lock);

        // No longer awaiting the lock.
        GSL_LOCAL()->awaitedLock = NULL;

acquired:
        // Record that I'm holding this lock mode now.
        *holding = lcAdd(procCounts, increment);
        return true;
}

/**
 * Release the given lock from the given mode and grant locks to any
 * processes that were blocked because of conflicts with this mode.
 *
 * If lastLocalHold is non-NULL, it will be set to whether or not this
 * was the last hold on this lock (in any mode) by this process.
 *
 * If lastHold is non-NULL, it will be set to whether this process was
 * the last holder of this lock (and there are no waiters).  Note that
 * this does *not* indicate the lock is not held when this returns,
 * since another process could acquire the lock by the time this
 * returns.  However, it is useful as a hint that cleanup may be
 * necessary.
 */
void gslRelease(gslLock_t *lock, gslLockMode_t mode, bool *lastLocalHold, bool *lastHold)
{
        GSL_INC_COUNTER(releaseCalls);
        assert(!GSL_LOCAL()->awaitedLock);
        assert(lcGetActive(lcGet(&lock->c), mode));

        // Atomically decrement my mode count and get all of the mode
        // counts.
        lockCounts_t increment = lcActive(mode);
        // XXX Doesn't need to be atomic?
retry:;
        lockCounts_t preCounts = lcGet(&lock->c);
        lockCounts_t postCounts = lcSubtract(preCounts, increment);
        if (!lcSet(&lock->c, preCounts, postCounts))
                goto retry;
        GSL_PP();

        // Record that I'm no longer holding this lock mode.
        lockCounts_t *holding = &GSL_PERLOCK(lock)->holding;
        *holding = lcSubtract(*holding, increment);
        if (lastLocalHold)
                *lastLocalHold = lcZero(*holding);

        // Unfortunately, in the presence of mode upgrades, my mode
        // count going to zero is *not* a correct test for whether
        // it's possible I should wake waiters.  If another process
        // holds this mode, but was blocked by me on a *higher* mode,
        // the mode count won't be zero, but I still need to wake it.

        // Check if there are waiters for any of the modes
        // that conflict with the mode I just released.
        lockCounts_t conflictMask =
                lcConflictMask(GSL_CONFLICT_TABLE(lock), mode, false, true);
        if (lcZero(lcAnd(postCounts, conflictMask))) {
                // There are no waiters I could possibly wake, so
                // don't bother.
                if (lastHold)
                        *lastHold = lcZero(postCounts);
                return;
        }

        // Some waiter in the queue conflicted with the lock
        // type I just released, so try doing a wake.
        GSL_MUTEX_ACQUIRE(&lock->queueLock);
        wake(lock);
        GSL_MUTEX_RELEASE(&lock->queueLock);
        if (lastHold)
                *lastHold = false;
}

/**
 * Cancel the acquisition of the given lock.  This only makes sense if
 * I was attempting to acquire this lock and was interrupted during
 * GSL_ACQUIRE_SLEEP (in fact, this will panic otherwise).  If this
 * successfully cancels the request for the lock, it returned true.
 * If this process was granted the lock in the mean time, this cleans
 * up the acquisition and returns false.
 *
 * If lastLocalHold is non-NULL and the acquisition gets canceled, it
 * will be set to whether or not this was the only attempt by this
 * process to acquire this lock.
 *
 * Note that if the acquisition gets canceled, it is impossible that
 * this was the last global holder of this lock, since the lock would
 * have been granted in that case.
 */
bool gslCancel(gslLock_t *lock, bool *lastLocalHold)
{
        // Really we should just use awaitedLock, but this sanity
        // checks that we're synched-up with our caller.
        assert(lock == GSL_LOCAL()->awaitedLock);

        gslLockMode_t wantMode = GSL_SHARED(GSL_CURPROC())->wantMode;

        // The only way to tell if I was given the lock or not is to
        // examine the wait list.  Take out the queue lock so the list
        // can't change under me.
        GSL_MUTEX_ACQUIRE(&lock->queueLock);

        if (GSL_SHARED(GSL_CURPROC())->lockNext == NULL) {
                // I was given the lock some time after sleep was
                // aborted.  This means I just didn't get a chance to
                // finish the local part of the acquisition.
                lockCounts_t *holding = &GSL_PERLOCK(lock)->holding;
                *holding = lcAdd(*holding, lcActive(wantMode));
                GSL_LOCAL()->awaitedLock = NULL;
                GSL_MUTEX_RELEASE(&lock->queueLock);
                return false;
        }

        // I'm still on the wait list.  Remove myself and fix the
        // counters.  Unfortunately, it's a singly-linked list.
        gslProc_t **prevLink = &lock->waiter;
        gslProc_t *proc = *prevLink;
        for (; proc; prevLink = &GSL_SHARED(proc)->lockNext, proc = *prevLink) {
                if (proc == GSL_CURPROC()) {
                        // Found me.  Remove me.
                        *prevLink = GSL_SHARED(GSL_CURPROC())->lockNext;
                        GSL_SHARED(GSL_CURPROC())->lockNext = NULL;
                        break;
                }
        }
        assert(proc);
        assert(lock->waitCounts[wantMode]);
        --lock->waitCounts[wantMode];

        if (lastLocalHold)
                *lastLocalHold = lcZero(GSL_PERLOCK(lock)->holding);

        GSL_MUTEX_RELEASE(&lock->queueLock);
        return true;
}
