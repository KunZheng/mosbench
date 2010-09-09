#ifndef PAREX_H
#define PAREX_H

#include <stdbool.h>
#include <pth.h>

#define TRACE() do { printf(__FILE__ ":%d (%s)\n", __LINE__, curProc ? curProc->name : "none"); } while (0)

enum procState_t {
        // In UNINITIALIZED, no pth thread is associated with this
        // proc.  In all other states, there is a pth thread.
        UNINITIALIZED,
        RUNNABLE,
        STOPPED,
        DONE
};

typedef struct proc_t
{
        // The name of this proc
        const char *name;
        // The entry point where this proc should begin execution
        void (*entry)(void);

        enum procState_t state;

        // The pth thread implementing this proc, if state !=
        // UNINITIALIZED.
        pth_t pth;

        // The sleep/wake count should be between -1 and 1, inclusive.
        // This lets us wake a proc before it sleeps and plays the
        // role of a wake-up semaphore.
        int sleepCount;

        // If this proc is waiting on a mutex, the next mutex in the
        // waiter list.
        struct proc_t *mutexNext;

        // Per-proc user state.  This is preserved across runs, so the
        // reset function should clear it if desired.
        void *user;
} proc_t;

extern proc_t *curProc;
extern proc_t *procs;
extern int nProcs;

#define XSTRINGIFY(x) #x
#define STRINGIFY(x) XSTRINGIFY(x)
#define swtch() do { _swtch(__FILE__ ":" STRINGIFY(__LINE__)); } while (0)

void procInit(void (*resetFn_)(void), int maxSwitches_);
proc_t *procAdd(const char *name, void (*entry)(void));
void procPrintPath(void);
void procRun(void);
void _swtch(const char *name);
void fail(const char *str);

void procSleep(void);
void procWake(proc_t *p);

typedef struct
{
        bool value;
        proc_t *waiter;
        proc_t **waiterTail;
} mutex_t;

void mutexLock(mutex_t *m);
void mutexRelease(mutex_t *m);

#endif
