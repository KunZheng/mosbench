#ifndef _BENCHLIB_H
#define _BENCHLIB_H

#include <stdbool.h>
#include <stdint.h>

// msg.c

void panic(const char *fmt, ...);
void epanic(const char *fmt, ...);

// args.c

enum benchOptType {
        BENCHOPT_BOOL,
        BENCHOPT_INT,
        BENCHOPT_STRING,
        BENCHOPT_CHOICES,
};

struct benchOpt
{
        const char *name;
        void *dest;
        enum benchOptType type;
        const char **choices;
};

void parseBenchOpts(const struct benchOpt *opts, int argc, char * const *argv);

// procs.c

int workerID;
struct
{
        volatile bool shutdown;
        // XXX Spread across cache lines?
        volatile int64_t counts[0];
} *sharedMem;

static inline bool
shutdownQueued(void)
{
        return sharedMem->shutdown;
}

static inline void
requestShutdown(void)
{
        sharedMem->shutdown = true;
}

static inline void
addToCount(int64_t n)
{
        sharedMem->counts[workerID] += n;
}

void createWorkerProcs(int num, void (*worker)(void));

#endif // _BENCHLIB_H
