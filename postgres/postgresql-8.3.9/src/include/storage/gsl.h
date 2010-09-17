#ifndef GSL_H_
#define GSL_H_

#ifdef LOCK_SCALABLE

#include "c.h"
#include "storage/lwlock.h"
#include "utils/hsearch.h"

typedef struct PGPROC gslProc_t;
typedef LWLockId gslMutex_t;
#include "../../../../gsl/lc-int128.h"
#include "../../../../gsl/gsl-header.h"

typedef struct GSLPERLOCK
{
	gslLock_t *tag;
	gslPerProcPerLock_t pl;
} GSLPERLOCK;

extern conflictTable_t LockGSLConflicts;
extern gslPerProcLocal_t LockGSLPerProc;
extern HTAB *LockGSLPerLock;

bool gslHolding(gslLock_t *l);

#endif

#endif
