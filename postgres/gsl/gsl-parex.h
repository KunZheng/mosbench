#ifndef GSL_PAREX_H
#define GSL_PAREX_H

#include "parex.h"

//#include "lc-array.h"
#include "lc-int128.h"

typedef proc_t gslProc_t;
typedef mutex_t gslMutex_t;

#include "gsl-header.h"

// Test-specific

extern gslLock_t theLock;
extern conflictTable_t conflictTable;

typedef struct {
        struct gslPerProcShared_t shared;
        struct gslPerProcLocal_t local;
        // For testing, there's only one lock
        struct gslPerProcPerLock_t perLock;
        // The modes this proc should test
        gslLockMode_t testMode1, testMode2;
        // Whether each lock should be taken in non-blocking mode
        bool testMode1DontWait, testMode2DontWait;
} testPerProc_t;

#define PERPROC(proc) ((testPerProc_t*)(proc)->user)

struct
{
        int acquireCalls;
        int releaseCalls;
        int wakeCalls;
        int spuriousWakes;
} counters;

#endif
