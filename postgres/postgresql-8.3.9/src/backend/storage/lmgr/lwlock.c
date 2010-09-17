/*-------------------------------------------------------------------------
 *
 * lwlock.c
 *	  Lightweight lock manager
 *
 * Lightweight locks are intended primarily to provide mutual exclusion of
 * access to shared-memory data structures.  Therefore, they offer both
 * exclusive and shared lock modes (to support read/write and read-only
 * access to a shared object).	There are few other frammishes.  User-level
 * locking should be done with the full lock manager --- which depends on
 * LWLocks to protect its shared state.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/storage/lmgr/lwlock.c,v 1.50.2.1 2009/03/11 00:08:07 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/clog.h"
#include "access/multixact.h"
#include "access/subtrans.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/spin.h"
#include "executor/instrument.h"

/* displays counters (number of lock acquires, etc)
 * on process exit */
//#define LWLOCK_STATS

#ifdef LWLOCK_STATS

/* also displays cumulative timings of time spent
 * waiting and holding locks
 * (depends on LWLOCK_STATS)
 */
//#define LWLOCK_TIMING_STATS

/* displays only locks which were waited on more than
 * this amount of seconds (set to 0 to display all)
 * (depends on LWLOCK_TIMING_STATS)
 */
#define LWLOCKS_STATS_TIME_DISPLAY_THRESHOLD 0.01
#endif /* LWLOCK_STATS */

//#define LWLOCK_SCALABLE 1

#ifdef LWLOCK_SCALABLE
# ifdef HAVE_LONG_INT_64
#  define SEM_COUNT_64 1
# endif
// XXX Follow Postgres naming conventions
typedef signed long int rwsem_count_t;

/* The bits in the active mask count the number of readers+writers.
 * This limits the count to 32767 for 32 bits and 2147483647 for 64
 * bits.
 */
# ifdef SEM_COUNT_64
#  define RWSEM_ACTIVE_MASK		0xffffffffL
# else
#  define RWSEM_ACTIVE_MASK		0x0000ffffL
# endif

# define RWSEM_UNLOCKED_VALUE		0x00000000L
# define RWSEM_ACTIVE_BIAS			0x00000001L
// XXX Why not just ~RWSEM_ACTIVE_MASK?
# define RWSEM_WAITING_BIAS			(-RWSEM_ACTIVE_MASK-1)
# define RWSEM_ACTIVE_READ_BIAS		RWSEM_ACTIVE_BIAS
# define RWSEM_ACTIVE_WRITE_BIAS	(RWSEM_WAITING_BIAS + RWSEM_ACTIVE_BIAS)

#endif



/* We use the ShmemLock spinlock to protect LWLockAssign */
extern slock_t *ShmemLock;


typedef struct LWLock
{
#ifdef LWLOCK_SCALABLE
	rwsem_count_t	count;
	slock_t			mutex;	/* Protects queue of PGPROCs */
#else
	slock_t		mutex;			/* Protects LWLock and queue of PGPROCs */
	bool		releaseOK;		/* T if ok to release waiters */
	char		exclusive;		/* # of exclusive holders (0 or 1) */
	int			shared;			/* # of shared holders (0..MaxBackends) */
#endif
	PGPROC	   *head;			/* head of list of waiting PGPROCs */
	PGPROC	   *tail;			/* tail of list of waiting PGPROCs */
	/* tail is undefined when head is NULL */
} LWLock;

/*
 * All the LWLock structs are allocated as an array in shared memory.
 * (LWLockIds are indexes into the array.)	We force the array stride to
 * be a power of 2, which saves a few cycles in indexing, but more
 * importantly also ensures that individual LWLocks don't cross cache line
 * boundaries.	This reduces cache contention problems, especially on AMD
 * Opterons.  (Of course, we have to also ensure that the array start
 * address is suitably aligned.)
 *
 * LWLock is between 16 and 32 bytes on all known platforms, so these two
 * cases are sufficient.
 */
#define LWLOCK_PADDED_SIZE	(sizeof(LWLock) <= 16 ? 16 : 32)

typedef union LWLockPadded
{
	LWLock		lock;
	char		pad[LWLOCK_PADDED_SIZE];
} LWLockPadded;

/*
 * This points to the array of LWLocks in shared memory.  Backends inherit
 * the pointer by fork from the postmaster (except in the EXEC_BACKEND case,
 * where we have special measures to pass it down).
 */
NON_EXEC_STATIC LWLockPadded *LWLockArray = NULL;


/*
 * We use this structure to keep track of locked LWLocks for release
 * during error recovery.  The maximum size could be determined at runtime
 * if necessary, but it seems unlikely that more than a few locks could
 * ever be held simultaneously.
 */
#define MAX_SIMUL_LWLOCKS	(100 + MAX_LOCK_PARTITIONS - 16)

static int	num_held_lwlocks = 0;
static LWLockId held_lwlocks[MAX_SIMUL_LWLOCKS];

#ifdef LWLOCK_SCALABLE
/*
 * We need to track whether we have each lock exclusive or shared so
 * we can release it appropriately.  This array parallels
 * held_lwlocks.
 */
static LWLockMode held_mode[MAX_SIMUL_LWLOCKS];
#endif

static int	lock_addin_request = 0;
static bool lock_addin_request_allowed = true;

#ifdef LOCK_DEBUG
bool		Trace_lwlocks = false;

inline static void
PRINT_LWDEBUG(const char *where, LWLockId lockid, const volatile LWLock *lock)
{
	if (Trace_lwlocks)
		elog(LOG, "%s(%d): excl %d shared %d head %p rOK %d",
			 where, (int) lockid,
			 (int) lock->exclusive, lock->shared, lock->head,
			 (int) lock->releaseOK);
}

inline static void
LOG_LWDEBUG(const char *where, LWLockId lockid, const char *msg)
{
	if (Trace_lwlocks)
		elog(LOG, "%s(%d): %s", where, (int) lockid, msg);
}
#else							/* not LOCK_DEBUG */
#define PRINT_LWDEBUG(a,b,c)
#define LOG_LWDEBUG(a,b,c)
#endif   /* LOCK_DEBUG */

#ifdef LWLOCK_STATS

typedef struct
{
	/* Process-local Counters for the number of times the lock has been ... */
	int		count_acquired_share;			/* acquired in SHARE mode */
	int		count_waited_share;				/* and it needed to wait */

	int		count_acquired_exclusive;		/*	acquired in EXCLUSIVEmode */
	int		count_waited_exclusive;			/* and it needed to wait */

#ifdef LWLOCK_TIMING_STATS
	/* Process local sum of time during which ... */
	instr_time		time_waited_share;		/* we waited to get a SHARE lock */
	instr_time		time_held_share;		/* we held the SHARE lock */
	instr_time		time_waited_exclusive;	/* we waited to get an EXCLUSIVE lock */
	instr_time		time_held_exclusive;	/* we held the EXCLUSIVE lock */

	instr_time		ta, th;	/* temp variables */
#endif
} LWLockStatsData;

static int	counts_for_pid = 0;
static LWLockStatsData *lwlock_stats;
static char **lwlock_names;
#ifdef LWLOCK_TIMING_STATS
static instr_time lwlock_stats_begin_time;
#endif

static const char* lwlock_stats_lock_name( LWLockId id )
{
	static const char * names[] = {
	"BufFreelist",
	"ShmemIndex",
	"OidGen",
	"XidGen",
	"ProcArray",
	"SInval",
	"FreeSpace",
	"WALInsert",
	"WALWrite",
	"ControlFile",
	"Checkpoint",
	"CLogControl",
	"SubtransControl",
	"MultiXactGen",
	"MultiXactOffsetControl",
	"MultiXactMemberControl",
	"RelCacheInit",
	"BgWriterComm",
	"TwoPhaseState",
	"TablespaceCreate",
	"BtreeVacuum",
	"AddinShmemInit",
	"Autovacuum",
	"AutovacuumSchedule",
	"SyncScan",
	"Logging" };

	if( id >= 0 && id < (sizeof( names )/sizeof( names[0] )))
		return names[id];
	else if (lwlock_names[id])
		return lwlock_names[id];
	else
		return "";
}

static void
print_lwlock_stats(int code, Datum arg)
{
	int			i, lines=0;
	int		   *LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
	int			numLocks = LWLockCounter[1];
#ifdef LWLOCK_TIMING_STATS
	instr_time	end_time;
	double		total_percents;
	INSTR_TIME_SET_CURRENT( end_time );
	INSTR_TIME_SUBTRACT( end_time, lwlock_stats_begin_time );
	total_percents = 100.0 / INSTR_TIME_GET_DOUBLE( end_time );
#endif

	if (!MyProc)
		// We can't safely take out locks here, which means both that
		// there's probably nothing to print, and that we can't take
		// the LoggingLock to print anything.
		return;

	/* Grab an LWLock to keep different backends from mixing reports */
	LWLockAcquire(LoggingLock, LW_EXCLUSIVE);

	fprintf( stderr, "\n-------- Lock stats for PID %d\n", MyProcPid );

	for (i = 0; i < numLocks; i++)
	{
		LWLockStatsData *p = &lwlock_stats[i];
#ifdef LWLOCK_TIMING_STATS
		double	time_waited_share		= INSTR_TIME_GET_DOUBLE(p->time_waited_share	);
		double	time_held_share			= INSTR_TIME_GET_DOUBLE(p->time_held_share		);
		double	time_waited_exclusive	= INSTR_TIME_GET_DOUBLE(p->time_waited_exclusive);
		double	time_held_exclusive		= INSTR_TIME_GET_DOUBLE(p->time_held_exclusive	);
#endif

		//if( !( p->count_waited_share || p->count_waited_exclusive ))
		if( !( p->count_acquired_share || p->count_acquired_exclusive ))
			continue;

#ifdef LWLOCK_TIMING_STATS
		if(    time_waited_share		< LWLOCKS_STATS_TIME_DISPLAY_THRESHOLD
			&& time_waited_exclusive	< LWLOCKS_STATS_TIME_DISPLAY_THRESHOLD )
			continue;
#endif

#ifdef LWLOCK_TIMING_STATS
		if( !(lines++ & 15) )
			fprintf( stderr, "    PID    Lock      ShAcq     ShWait     ShWaitT     ShHeldT      ExAcq     "
							 "ExWait             ExWaitT                ExHeldT     Name\n" );

		fprintf( stderr,
			"%7d %7d %10d %10d %11.02f %11.02f %10d %10d %11.02f (%6.02f %%) %11.02f (%6.02f %%)  %s\n",
			MyProcPid, i,
			p->count_acquired_share, p->count_waited_share, time_waited_share, time_held_share,
			p->count_acquired_exclusive, p->count_waited_exclusive,
			time_waited_exclusive, 	time_waited_exclusive * total_percents,
			time_held_exclusive,	time_held_exclusive * total_percents,
			lwlock_stats_lock_name( i ));
#else
		if( !(lines++ & 15) )
			fprintf( stderr, "    PID    Lock      ShAcq     ShWait     ExAcq     ExWait     Name\n" );

		fprintf( stderr,
			"%7d %7d %10d %10d %10d %10d    %s\n",
			MyProcPid, i,
			p->count_acquired_share, p->count_waited_share,
			p->count_acquired_exclusive, p->count_waited_exclusive,
			lwlock_stats_lock_name( i ));
#endif
	}

	LWLockRelease(LoggingLock);
}

static void lock_stats_setup( void )
{
	int		   *LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
	int			numLocks = LWLockCounter[1];

	lwlock_stats = calloc(numLocks, sizeof(LWLockStatsData));
	counts_for_pid = MyProcPid;

#ifdef LWLOCK_TIMING_STATS
	INSTR_TIME_SET_CURRENT( lwlock_stats_begin_time );
#endif

	on_shmem_exit(print_lwlock_stats, 0);
}
#endif   /* LWLOCK_STATS */


/*
 * Compute number of LWLocks to allocate.
 */
int
NumLWLocks(void)
{
	int			numLocks;

	/*
	 * Possibly this logic should be spread out among the affected modules,
	 * the same way that shmem space estimation is done.  But for now, there
	 * are few enough users of LWLocks that we can get away with just keeping
	 * the knowledge here.
	 */

	/* Predefined LWLocks */
	numLocks = (int) NumFixedLWLocks;

	/* lock.c needs one per lock partition */
	numLocks += NUM_LOCK_PARTITIONS;

	/* bufmgr.c needs two for each shared buffer */
	numLocks += 2 * NBuffers;

	/* clog.c needs one per CLOG buffer */
	numLocks += NUM_CLOG_BUFFERS;

	/* subtrans.c needs one per SubTrans buffer */
	numLocks += NUM_SUBTRANS_BUFFERS;

	/* multixact.c needs two SLRU areas */
	numLocks += NUM_MXACTOFFSET_BUFFERS + NUM_MXACTMEMBER_BUFFERS;

	/*
	 * Add any requested by loadable modules; for backwards-compatibility
	 * reasons, allocate at least NUM_USER_DEFINED_LWLOCKS of them even if
	 * there are no explicit requests.
	 */
	lock_addin_request_allowed = false;
	numLocks += Max(lock_addin_request, NUM_USER_DEFINED_LWLOCKS);

	return numLocks;
}


/*
 * RequestAddinLWLocks
 *		Request that extra LWLocks be allocated for use by
 *		a loadable module.
 *
 * This is only useful if called from the _PG_init hook of a library that
 * is loaded into the postmaster via shared_preload_libraries.	Once
 * shared memory has been allocated, calls will be ignored.  (We could
 * raise an error, but it seems better to make it a no-op, so that
 * libraries containing such calls can be reloaded if needed.)
 */
void
RequestAddinLWLocks(int n)
{
	if (IsUnderPostmaster || !lock_addin_request_allowed)
		return;					/* too late */
	lock_addin_request += n;
}


/*
 * Compute shmem space needed for LWLocks.
 */
Size
LWLockShmemSize(void)
{
	Size		size;
	int			numLocks = NumLWLocks();

	/* Space for the LWLock array. */
	size = mul_size(numLocks, sizeof(LWLockPadded));

	/* Space for dynamic allocation counter, plus room for alignment. */
	size = add_size(size, 2 * sizeof(int) + LWLOCK_PADDED_SIZE);

	return size;
}


/*
 * Allocate shmem space for LWLocks and initialize the locks.
 */
void
CreateLWLocks(void)
{
	int			numLocks = NumLWLocks();
	Size		spaceLocks = LWLockShmemSize();
	LWLockPadded *lock;
	int		   *LWLockCounter;
	char	   *ptr;
	int			id;

	/* Allocate space */
	ptr = (char *) ShmemAlloc(spaceLocks);

	/* Leave room for dynamic allocation counter */
	ptr += 2 * sizeof(int);

	/* Ensure desired alignment of LWLock array */
	ptr += LWLOCK_PADDED_SIZE - ((unsigned long) ptr) % LWLOCK_PADDED_SIZE;

	LWLockArray = (LWLockPadded *) ptr;

	/*
	 * Initialize all LWLocks to "unlocked" state
	 */
	for (id = 0, lock = LWLockArray; id < numLocks; id++, lock++)
	{
		SpinLockInit(&lock->lock.mutex);
#ifdef LWLOCK_SCALABLE
		lock->lock.count = RWSEM_UNLOCKED_VALUE;
#else
		lock->lock.releaseOK = true;
		lock->lock.exclusive = 0;
		lock->lock.shared = 0;
#endif
		lock->lock.head = NULL;
		lock->lock.tail = NULL;
	}

	/*
	 * Initialize the dynamic-allocation counter, which is stored just before
	 * the first LWLock.
	 */
	LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
	LWLockCounter[0] = (int) NumFixedLWLocks;
	LWLockCounter[1] = numLocks;

#ifdef LWLOCK_STATS
	lwlock_names = calloc(numLocks, sizeof *lwlock_names);
	for (id = 0; id < NUM_BUFFER_PARTITIONS; id++)
		asprintf(&lwlock_names[id + FirstBufMappingLock],
				 "BufMapping#%d/%d", id, NUM_BUFFER_PARTITIONS);
#endif
}


/*
 * LWLockAssign - assign a dynamically-allocated LWLock number
 *
 * We interlock this using the same spinlock that is used to protect
 * ShmemAlloc().  Interlocking is not really necessary during postmaster
 * startup, but it is needed if any user-defined code tries to allocate
 * LWLocks after startup.
 */
LWLockId
LWLockAssign(const char *name)
{
	LWLockId	result;

	/* use volatile pointer to prevent code rearrangement */
	volatile int *LWLockCounter;

	LWLockCounter = (int *) ((char *) LWLockArray - 2 * sizeof(int));
	SpinLockAcquire(ShmemLock);
	if (LWLockCounter[0] >= LWLockCounter[1])
	{
		SpinLockRelease(ShmemLock);
		elog(ERROR, "no more LWLockIds available");
	}
	result = (LWLockId) (LWLockCounter[0]++);
	SpinLockRelease(ShmemLock);
#ifdef LWLOCK_STATS
	lwlock_names[result] = strdup(name);
#endif
	return result;
}


#ifdef LWLOCK_SCALABLE
static inline rwsem_count_t
LWLockAtomicUpdate(volatile LWLock *lock, rwsem_count_t delta)
{
	rwsem_count_t tmp = delta;

	asm volatile("lock xadd %0,%1"
				 : "+r" (tmp), "+m" (lock->count)
				 : : "memory", "cc");

	// tmp now holds the old value of the lock.  Return the new value.
	return tmp + delta;
}

// The caller *must* hold lock->mutex
static PGPROC*
LWLockGetWakeChain(volatile LWLock *lock)
{
	PGPROC	   *head;
	PGPROC	   *proc;

	for (;;) {
		rwsem_count_t old = LWLockAtomicUpdate(lock, RWSEM_ACTIVE_BIAS) - RWSEM_ACTIVE_BIAS;
		if (!(old & RWSEM_ACTIVE_MASK))
			// The lock is not in use.  Wake waiters.
			break;
		// The lock is still in use.  Undo our change.
		old = LWLockAtomicUpdate(lock, -RWSEM_ACTIVE_BIAS);
		if (old != 0)
			return NULL;
		// Now it looks like it isn't in use.  Try again.
	}

	head = lock->head;
	Assert(head);

	/*
	 * Remove the to-be-awakened PGPROCs from the queue.  If the front
	 * waiter wants exclusive lock, awaken him only. Otherwise awaken
	 * as many waiters as want shared access.
	 */
	proc = head;
	if (!proc->lwExclusive)
	{
		int		count = 1;
		while (proc->lwWaitLink != NULL &&
			   !proc->lwWaitLink->lwExclusive) {
			++count;
			proc = proc->lwWaitLink;
		}
		// Update lock count.  I already incremented the active count
		// above, so don't repeat that here.
		LWLockAtomicUpdate(lock, count * (RWSEM_ACTIVE_BIAS - RWSEM_WAITING_BIAS) - RWSEM_ACTIVE_BIAS);
	}
	/* proc is now the last PGPROC to be released */
	lock->head = proc->lwWaitLink;
	proc->lwWaitLink = NULL;

	/* Return the original head of the wake queue */
	return head;
}

static void
LWLockWakeChain(PGPROC *head)
{
	PGPROC *proc;

	while (head != NULL)
	{
		LOG_LWDEBUG("LWLockRelease", lockid, "release waiter");
		proc = head;
		head = proc->lwWaitLink;
		proc->lwWaitLink = NULL;
		proc->lwWaiting = false;
		PGSemaphoreUnlock(&proc->sem);
	}
}
#endif


/*
 * LWLockAcquire - acquire a lightweight lock in the specified mode
 *
 * If the lock is not available, sleep until it is.
 *
 * Side effect: cancel/die interrupts are held off until lock release.
 */
void
LWLockAcquire(LWLockId lockid, LWLockMode mode)
{
	volatile LWLock *lock = &(LWLockArray[lockid].lock);
	PGPROC	   *proc = MyProc;
	bool		retry = false;
	int			extraWaits = 0;
#ifdef LWLOCK_SCALABLE
	PGPROC	   *wakeChain = NULL;
#endif

	PRINT_LWDEBUG("LWLockAcquire", lockid, lock);

#ifdef LWLOCK_STATS
	/* Set up local count state first time through in a given process */
	if (counts_for_pid != MyProcPid)
		lock_stats_setup();

	/* Count lock acquisition attempts */
	if (mode == LW_EXCLUSIVE)	lwlock_stats[lockid].count_acquired_exclusive++;
	else						lwlock_stats[lockid].count_acquired_share++;
#ifdef LWLOCK_TIMING_STATS
	/* record start of wait time and lock mode */
	INSTR_TIME_SET_CURRENT( lwlock_stats[lockid].ta );
#endif
#endif   /* LWLOCK_STATS */

	/*
	 * We can't wait if we haven't got a PGPROC.  This should only occur
	 * during bootstrap or shared memory initialization.  Put an Assert here
	 * to catch unsafe coding practices.
	 */
	Assert(!(proc == NULL && IsUnderPostmaster));

	/* Ensure we will have room to remember the lock */
	if (num_held_lwlocks >= MAX_SIMUL_LWLOCKS)
		elog(ERROR, "too many LWLocks taken");

	/*
	 * Lock out cancel/die interrupts until we exit the code section protected
	 * by the LWLock.  This ensures that interrupts will not interfere with
	 * manipulations of data structures in shared memory.
	 */
	HOLD_INTERRUPTS();

	/*
	 * Loop here to try to acquire lock after each time we are signaled by
	 * LWLockRelease.
	 *
	 * NOTE: it might seem better to have LWLockRelease actually grant us the
	 * lock, rather than retrying and possibly having to go back to sleep. But
	 * in practice that is no good because it means a process swap for every
	 * lock acquisition when two or more processes are contending for the same
	 * lock.  Since LWLocks are normally used to protect not-very-long
	 * sections of computation, a process needs to be able to acquire and
	 * release the same lock many times during a single CPU time slice, even
	 * in the presence of contention.  The efficiency of being able to do that
	 * outweighs the inefficiency of sometimes wasting a process dispatch
	 * cycle because the lock is not free when a released waiter finally gets
	 * to run.	See pgsql-hackers archives for 29-Dec-01.
	 */
	for (;;)
	{
		bool		mustwait;
#ifdef LWLOCK_SCALABLE
		rwsem_count_t	adjustment;

		mustwait = false;

		if (mode == LW_EXCLUSIVE) {
			// See __down_write
			rwsem_count_t tmp;
			tmp = RWSEM_ACTIVE_WRITE_BIAS;
			asm volatile("# beginning down_write\n\t"
						 // tmp = count; count += RWSEM_ACTIVE_WRITE_BIAS
						 // Note that RWSEM_ACTIVE_WRITE_BIAS is a
						 // large negative number.
						 "lock xadd      %1,(%2)\n\t"
						 : "+m" (lock->count), "=d" (tmp)
						 : "a" (lock), "1" (tmp)
						 : "memory", "cc");

			// Was the count 0 before?
			if (tmp) {
				mustwait = true;
				adjustment = -RWSEM_ACTIVE_BIAS;
			}
		} else {
			// See __down_read
			asm volatile("# beginning down_read\n\t"
						 // Increment count
#ifdef SEM_COUNT_64
						 "lock incq (%2)\n\t"
#else
						 "lock incl (%2)\n\t"
#endif
						 // If the new count >= 0, then I got it.
						 "  jns        1f\n\t"
						 // The new count is negative, so there's a
						 // writer and I must wait.
						 "  movb       $1, %1\n"
						 "1:\n\t"
						 "# ending down_read\n\t"
						 : "+m" (lock->count), "+g" (mustwait)
						 : "a" (lock)
						 : "memory", "cc");
			adjustment = RWSEM_WAITING_BIAS - RWSEM_ACTIVE_BIAS;
		}

		if (!mustwait)
			break;				/* got the lock */

		/* Acquire mutex so we can add ourselves to the wait queue */
		SpinLockAcquire(&lock->mutex);
#else
		/* Acquire mutex.  Time spent holding mutex should be short! */
		SpinLockAcquire(&lock->mutex);

		/* If retrying, allow LWLockRelease to release waiters again */
		if (retry)
			lock->releaseOK = true;

		/* If I can get the lock, do so quickly. */
		if (mode == LW_EXCLUSIVE)
		{
			if (lock->exclusive == 0 && lock->shared == 0)
			{
				lock->exclusive++;
				mustwait = false;
			}
			else
				mustwait = true;
		}
		else
		{
			if (lock->exclusive == 0)
			{
				lock->shared++;
				mustwait = false;
			}
			else
				mustwait = true;
		}

		if (!mustwait)
			break;				/* got the lock */
#endif

		/*
		 * Add myself to wait queue.
		 *
		 * If we don't have a PGPROC structure, there's no way to wait. This
		 * should never occur, since MyProc should only be null during shared
		 * memory initialization.
		 */
		if (proc == NULL)
			elog(PANIC, "cannot wait without a PGPROC structure");

		proc->lwWaiting = true;
		proc->lwExclusive = (mode == LW_EXCLUSIVE);
		proc->lwWaitLink = NULL;
		if (lock->head == NULL)
			lock->head = proc;
		else
			lock->tail->lwWaitLink = proc;
		lock->tail = proc;

#ifdef LWLOCK_SCALABLE
		/* Now that we're waiting on the lock, fix changes we made to
		 * the semaphore count.
		 */
		if (!(LWLockAtomicUpdate(lock, adjustment) & RWSEM_ACTIVE_MASK))
			/* There are no active locks, so wake it up */
			// See rwsem_down_failed_common
			wakeChain = LWLockGetWakeChain(lock);
#endif

		/* Can release the mutex now */
		SpinLockRelease(&lock->mutex);

#ifdef LWLOCK_SCALABLE
		LWLockWakeChain(wakeChain);
#endif

		/*
		 * Wait until awakened.
		 *
		 * Since we share the process wait semaphore with the regular lock
		 * manager and ProcWaitForSignal, and we may need to acquire an LWLock
		 * while one of those is pending, it is possible that we get awakened
		 * for a reason other than being signaled by LWLockRelease. If so,
		 * loop back and wait again.  Once we've gotten the LWLock,
		 * re-increment the sema by the number of additional signals received,
		 * so that the lock manager or signal manager will see the received
		 * signal when it next waits.
		 */
		LOG_LWDEBUG("LWLockAcquire", lockid, "waiting");

#ifdef LWLOCK_STATS
		/* count lock waits */
		if (mode == LW_EXCLUSIVE)	lwlock_stats[lockid].count_waited_exclusive++;
		else						lwlock_stats[lockid].count_waited_share++;
#endif

		PG_TRACE2(lwlock__startwait, lockid, mode);

		for (;;)
		{
			/* "false" means cannot accept cancel/die interrupt here. */
			PGSemaphoreLock(&proc->sem, false);
			if (!proc->lwWaiting)
				break;
			extraWaits++;
		}

		PG_TRACE2(lwlock__endwait, lockid, mode);

		LOG_LWDEBUG("LWLockAcquire", lockid, "awakened");

#ifdef LWLOCK_SCALABLE
		/* In the scalable lock case, the releasing process *does*
		 * acquire the lock for us, so we shouldn't try again */
		break;
#else
		/* Now loop back and try to acquire lock again. */
		retry = true;
#endif
	}

#ifndef LWLOCK_SCALABLE
	/* We are done updating shared state of the lock itself. */
	SpinLockRelease(&lock->mutex);
#endif

	PG_TRACE2(lwlock__acquire, lockid, mode);

	/* Add lock to list of locks held by this backend */
#ifdef LWLOCK_SCALABLE
	held_mode[num_held_lwlocks] = mode;
#endif
	held_lwlocks[num_held_lwlocks++] = lockid;

	/*
	 * Fix the process wait semaphore's count for any absorbed wakeups.
	 */
	while (extraWaits-- > 0)
		PGSemaphoreUnlock(&proc->sem);

#ifdef LWLOCK_TIMING_STATS
	/* record end of wait time and start of hold time */
	INSTR_TIME_SET_CURRENT( lwlock_stats[lockid].th );

	if (mode == LW_EXCLUSIVE)	INSTR_TIME_ACCUM_DIFF( lwlock_stats[lockid].time_waited_exclusive, lwlock_stats[lockid].th, lwlock_stats[lockid].ta );
	else						INSTR_TIME_ACCUM_DIFF( lwlock_stats[lockid].time_waited_share,     lwlock_stats[lockid].th, lwlock_stats[lockid].ta );
#endif
}

/*
 * LWLockConditionalAcquire - acquire a lightweight lock in the specified mode
 *
 * If the lock is not available, return FALSE with no side-effects.
 *
 * If successful, cancel/die interrupts are held off until lock release.
 */
bool
LWLockConditionalAcquire(LWLockId lockid, LWLockMode mode)
{
	volatile LWLock *lock = &(LWLockArray[lockid].lock);
	bool		mustwait;

	PRINT_LWDEBUG("LWLockConditionalAcquire", lockid, lock);

	/* Ensure we will have room to remember the lock */
	if (num_held_lwlocks >= MAX_SIMUL_LWLOCKS)
		elog(ERROR, "too many LWLocks taken");

	/*
	 * Lock out cancel/die interrupts until we exit the code section protected
	 * by the LWLock.  This ensures that interrupts will not interfere with
	 * manipulations of data structures in shared memory.
	 */
	HOLD_INTERRUPTS();

#ifdef LWLOCK_SCALABLE
	if (mode == LW_EXCLUSIVE)
	{
		// See __down_write_trylock
		rwsem_count_t prev, old = RWSEM_UNLOCKED_VALUE,
			new = RWSEM_ACTIVE_WRITE_BIAS;

		// If the count is unlocked, set it to the write bias
		asm volatile(
# ifdef SEM_COUNT_64
			"lock cmpxchgq %1,%2\n\t"
# else
			"lock cmpxchgl %1,%2\n\t"
# endif
			: "=a" (prev)
			: "r" (new), "m" ((unsigned long)lock->count), "0" (old)
			: "memory", "cc");

		if (prev == RWSEM_UNLOCKED_VALUE)
			mustwait = false;
		else
			mustwait = true;
	} else {
		// See __down_read_trylock
		rwsem_count_t result, tmp;
		// If count is non-negative, increment it.  Standard lock-free
		// approach.
		asm volatile("# beginning __down_read_trylock\n\t"
					 "  mov          %0,%1\n\t"
					 "1:\n\t"
					 "  mov          %1,%2\n\t"
					 "  add          %3,%2\n\t"
					 "  jle	         2f\n\t"
					 "  lock cmpxchg %2,%0\n\t"
					 "  jnz	         1b\n\t"
					 "2:\n\t"
					 "# ending __down_read_trylock\n\t"
					 : "+m" (lock->count), "=&a" (result), "=&r" (tmp)
					 : "i" (RWSEM_ACTIVE_READ_BIAS)
					 : "memory", "cc");
		// If the count was non-negative, then I got it
		if (result >= 0)
			mustwait = false;
		else
			mustwait = true;
	}
#else
	/* Acquire mutex.  Time spent holding mutex should be short! */
	SpinLockAcquire(&lock->mutex);

	/* If I can get the lock, do so quickly. */
	if (mode == LW_EXCLUSIVE)
	{
		if (lock->exclusive == 0 && lock->shared == 0)
		{
			lock->exclusive++;
			mustwait = false;
		}
		else
			mustwait = true;
	}
	else
	{
		if (lock->exclusive == 0)
		{
			lock->shared++;
			mustwait = false;
		}
		else
			mustwait = true;
	}

	/* We are done updating shared state of the lock itself. */
	SpinLockRelease(&lock->mutex);
#endif

	if (mustwait)
	{
		/* Failed to get lock, so release interrupt holdoff */
		RESUME_INTERRUPTS();
		LOG_LWDEBUG("LWLockConditionalAcquire", lockid, "failed");
		PG_TRACE2(lwlock__condacquire__fail, lockid, mode);
	}
	else
	{
		/* Add lock to list of locks held by this backend */
#ifdef LWLOCK_SCALABLE
		held_mode[num_held_lwlocks] = mode;
#endif
		held_lwlocks[num_held_lwlocks++] = lockid;
		PG_TRACE2(lwlock__condacquire, lockid, mode);

#ifdef LWLOCK_STATS
		/* Set up local count state first time through in a given process */
		if (counts_for_pid != MyProcPid)
			lock_stats_setup();

		/* Count lock acquisition attempts */
		if (mode == LW_EXCLUSIVE)	lwlock_stats[lockid].count_acquired_exclusive++;
		else						lwlock_stats[lockid].count_acquired_share++;
#ifdef LWLOCK_TIMING_STATS
		/* record start of hold time */
		INSTR_TIME_SET_CURRENT( lwlock_stats[lockid].th );
#endif
#endif   /* LWLOCK_STATS */
	}

	return !mustwait;
}

/*
 * LWLockRelease - release a previously acquired lock
 */
void
LWLockRelease(LWLockId lockid)
{
	volatile LWLock *lock = &(LWLockArray[lockid].lock);
	PGPROC	   *head;
	PGPROC	   *proc;
	int			i;
#ifdef LWLOCK_TIMING_STATS
	bool		was_exclusive = false;
#endif
#ifdef LWLOCK_SCALABLE
	LWLockMode	mode;
	bool		mustwake = false;
#endif

	PRINT_LWDEBUG("LWLockRelease", lockid, lock);

	/*
	 * Remove lock from list of locks held.  Usually, but not always, it will
	 * be the latest-acquired lock; so search array backwards.
	 */
	for (i = num_held_lwlocks; --i >= 0;)
	{
		if (lockid == held_lwlocks[i])
			break;
	}
	if (i < 0)
		elog(ERROR, "lock %d is not held", (int) lockid);
#ifdef LWLOCK_SCALABLE
	mode = held_mode[i];
#endif
	num_held_lwlocks--;
	for (; i < num_held_lwlocks; i++) {
		held_lwlocks[i] = held_lwlocks[i + 1];
#ifdef LWLOCK_SCALABLE
		held_mode[i] = held_mode[i + 1];
#endif
	}

#ifdef LWLOCK_SCALABLE
	if (mode == LW_EXCLUSIVE) {
		// See __up_write
		rwsem_count_t tmp;
		// XXX Could use GCC builtins
		asm volatile("# beginning __up_write\n\t"
					 "  lock xadd      %1,(%3)\n\t"
					 /* tries to transition
						0xffff0001 -> 0x00000000 */
					 "  jz       1f\n\t"
					 // We transitioned to something else.  There are
					 // waiters.
					 "  movb     $1, %2\n\t"
					 "1:\n\t"
					 "# ending __up_write\n"
					 : "+m" (lock->count), "=d" (tmp), "+g" (mustwake)
					   // XXX What isn't tmp = -RWSEM_ACTIVE_WRITE_BIAS?
					 : "a" (lock), "1" (-RWSEM_ACTIVE_WRITE_BIAS)
					 : "memory", "cc");
#ifdef LWLOCK_TIMING_STATS
		was_exclusive = true;
#endif
	} else {
		// See __up_read
		rwsem_count_t tmp = -RWSEM_ACTIVE_READ_BIAS;
		// XXX Why not just lock dec?
		// XXX Could use GCC builtins
		asm volatile("# beginning __up_read\n\t"
					 // tmp = count; count--
					 "  lock xadd      %1,(%3)\n\t"
					 // If new count >= 0, there's nobody to wake
					 "  jns        1f\n\t"
					 // New count < 0, there's a writer to wake
					 "  movb       $1, %2\n\t"
					 "1:\n"
					 "# ending __up_read\n"
					 : "+m" (lock->count), "=d" (tmp), "+g" (mustwake)
					 : "a" (lock), "1" (tmp)
					 : "memory", "cc");
	}

	if (mustwake) {
		// XXX Very redundant with code below
		// See rwsem_wake
		SpinLockAcquire(&lock->mutex);
		if (lock->head)
			head = LWLockGetWakeChain(lock);
		else
			head = NULL;
		SpinLockRelease(&lock->mutex);
		PG_TRACE1(lwlock__release, lockid);
		LWLockWakeChain(head);
	}
#else
	/* Acquire mutex.  Time spent holding mutex should be short! */
	SpinLockAcquire(&lock->mutex);

	/* Release my hold on lock */
	if (lock->exclusive > 0)
	{
		lock->exclusive--;
#ifdef LWLOCK_TIMING_STATS
		was_exclusive = true;
#endif
	}
	else
	{
		Assert(lock->shared > 0);
		lock->shared--;
	}

	/*
	 * See if I need to awaken any waiters.  If I released a non-last shared
	 * hold, there cannot be anything to do.  Also, do not awaken any waiters
	 * if someone has already awakened waiters that haven't yet acquired the
	 * lock.
	 */
	head = lock->head;
	if (head != NULL)
	{
		if (lock->exclusive == 0 && lock->shared == 0 && lock->releaseOK)
		{
			/*
			 * Remove the to-be-awakened PGPROCs from the queue.  If the front
			 * waiter wants exclusive lock, awaken him only. Otherwise awaken
			 * as many waiters as want shared access.
			 */
			proc = head;
			if (!proc->lwExclusive)
			{
				while (proc->lwWaitLink != NULL &&
					   !proc->lwWaitLink->lwExclusive)
					proc = proc->lwWaitLink;
			}
			/* proc is now the last PGPROC to be released */
			lock->head = proc->lwWaitLink;
			proc->lwWaitLink = NULL;
			/* prevent additional wakeups until retryer gets to run */
			lock->releaseOK = false;
		}
		else
		{
			/* lock is still held, can't awaken anything */
			head = NULL;
		}
	}

	/* We are done updating shared state of the lock itself. */
	SpinLockRelease(&lock->mutex);

	PG_TRACE1(lwlock__release, lockid);

	/*
	 * Awaken any waiters I removed from the queue.
	 */
	while (head != NULL)
	{
		LOG_LWDEBUG("LWLockRelease", lockid, "release waiter");
		proc = head;
		head = proc->lwWaitLink;
		proc->lwWaitLink = NULL;
		proc->lwWaiting = false;
		PGSemaphoreUnlock(&proc->sem);
	}
#endif

	/*
	 * Now okay to allow cancel/die interrupts.
	 */
	RESUME_INTERRUPTS();
#ifdef LWLOCK_TIMING_STATS
	/* record end of hold time */
	INSTR_TIME_SET_CURRENT( lwlock_stats[lockid].ta );

	if (was_exclusive)	INSTR_TIME_ACCUM_DIFF( lwlock_stats[lockid].time_held_exclusive, lwlock_stats[lockid].ta, lwlock_stats[lockid].th );
	else				INSTR_TIME_ACCUM_DIFF( lwlock_stats[lockid].time_held_share,     lwlock_stats[lockid].ta, lwlock_stats[lockid].th );
#endif
}


/*
 * LWLockReleaseAll - release all currently-held locks
 *
 * Used to clean up after ereport(ERROR). An important difference between this
 * function and retail LWLockRelease calls is that InterruptHoldoffCount is
 * unchanged by this operation.  This is necessary since InterruptHoldoffCount
 * has been set to an appropriate level earlier in error recovery. We could
 * decrement it below zero if we allow it to drop for each released lock!
 */
void
LWLockReleaseAll(void)
{
	while (num_held_lwlocks > 0)
	{
		HOLD_INTERRUPTS();		/* match the upcoming RESUME_INTERRUPTS */

		LWLockRelease(held_lwlocks[num_held_lwlocks - 1]);
	}
}


/*
 * LWLockHeldByMe - test whether my process currently holds a lock
 *
 * This is meant as debug support only.  We do not distinguish whether the
 * lock is held shared or exclusive.
 */
bool
LWLockHeldByMe(LWLockId lockid)
{
	int			i;

	for (i = 0; i < num_held_lwlocks; i++)
	{
		if (held_lwlocks[i] == lockid)
			return true;
	}
	return false;
}
