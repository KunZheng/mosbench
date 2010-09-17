#ifndef LC_ARRAY_H
#define LC_ARRAY_H

#include <assert.h>

#define LC_MAX_MODES 8          /* Arbitrary */

typedef struct
{
        unsigned int active[LC_MAX_MODES];
        bool waiting[LC_MAX_MODES];
} lockCounts_t;

//////////////////////////////////////////////////////////////////
// Lock counts
//

const lockCounts_t LC_ZERO;

static inline lockCounts_t lcGet(lockCounts_t *c)
{
        return *c;
}

static inline bool lcSet(lockCounts_t *c, lockCounts_t old, lockCounts_t new)
{
        for (int i = 0; i < LC_MAX_MODES; ++i)
                if (c->active[i] != old.active[i] ||
                    c->waiting[i] != old.waiting[i])
                        return false;
        *c = new;
        return true;
}

// Constructors

static inline lockCounts_t lcActive(gslLockMode_t mode)
{
        lockCounts_t res = LC_ZERO;
        res.active[mode] = 1;
        return res;
}

static inline lockCounts_t lcSetWait(lockCounts_t c, gslLockMode_t mode, bool wait)
{
        c.waiting[mode] = wait;
        return c;
}

// Arithmetic

static inline lockCounts_t lcAdd(lockCounts_t a, lockCounts_t b)
{
        lockCounts_t res;
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                res.active[i] = a.active[i] + b.active[i];
                assert(!(a.waiting[i] && b.waiting[i]));
                res.waiting[i] = a.waiting[i] || b.waiting[i];
        }
        return res;
}

static inline lockCounts_t lcSubtract(lockCounts_t a, lockCounts_t b)
{
        lockCounts_t res;
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                assert(a.active[i] >= b.active[i]);
                res.active[i] = a.active[i] - b.active[i];
                assert(a.waiting[i] >= b.waiting[i]);
                res.waiting[i] = a.waiting[i] - b.waiting[i];
        }
        return res;
}

// Decomposition

static inline bool lcNonZero(lockCounts_t c)
{
        for (int i = 0; i < LC_MAX_MODES; ++i)
                if (c.active[i] || c.waiting[i])
                        return true;
        return false;
}

static inline bool lcZero(lockCounts_t c)
{
        return !lcNonZero(c);
}

static inline int lcGetActive(lockCounts_t c, gslLockMode_t mode)
{
        return c.active[mode];
}

// Masks

static inline lockCounts_t lcConflictMask(gslLockMode_t mode, bool active, bool waiting)
{
#define CONFLICTS(a, b) (conflicts[a] & (1<<(b)))
        lockCounts_t res = LC_ZERO;
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                if (CONFLICTS(mode, i)) {
                        if (active)
                                res.active[i] = ~0;
                        if (waiting)
                                res.waiting[i] = true;
                }
        }
        return res;
#undef CONFLICTS
}

static inline lockCounts_t lcAnd(lockCounts_t a, lockCounts_t b)
{
        lockCounts_t res;
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                res.active[i] = a.active[i] & b.active[i];
                res.waiting[i] = a.waiting[i] & b.waiting[i];
        }
        return res;
}

static inline lockCounts_t lcOr(lockCounts_t a, lockCounts_t b)
{
        lockCounts_t res;
        for (int i = 0; i < LC_MAX_MODES; ++i) {
                res.active[i] = a.active[i] | b.active[i];
                res.waiting[i] = a.waiting[i] | b.waiting[i];
        }
        return res;
}

static inline lockCounts_t lcMaskSet(void)
{
        static lockCounts_t res;
        if (res.active[0] == 0) {
                for (int i = 0; i < LC_MAX_MODES; ++i) {
                        res.active[i] = ~0;
                        res.waiting[i] = true;
                }
        }
        return res;
}

#endif
