#include <assert.h>

#include "lc-int128.h"

#define LC_BITS_PER_MODE 16
#define LC_ACTIVE_MASK   ((__uint128_t)0x7FFF)
#define LC_WAIT_MASK     ((__uint128_t)0x8000)

const lockCounts_t LC_ZERO;

void lcInitConflictTable(conflictTable_t *m, int *conflicts, int nConflicts)
{
        assert(LC_MAX_MODES <= (8 * sizeof(lockCounts_t)) / LC_BITS_PER_MODE);

        for (int i = 0; i < LC_MAX_MODES; ++i) {
                __uint128_t bits = 0;
                if (i < nConflicts) {
                        for (int j = 0; j < nConflicts; ++j) {
                                if (conflicts[i] & (1 << j))
                                        bits |= (__uint128_t)1 << (j*LC_BITS_PER_MODE);
                        }
                }
                for (int active = 0; active <= 1; ++active)
                        for (int waiting = 0; waiting <= 1; ++waiting)
                                m->bits[active][waiting][i] =
                                        bits * ((active  ? LC_ACTIVE_MASK : 0) |
                                                (waiting ? LC_WAIT_MASK   : 0));
        }
}

static inline lockCounts_t lcGet(lockCounts_t *c)
{
        // Perform an atomic 128-bit load
        lockCounts_t res;
        asm volatile ("movdqa %1, %0" : "=x" (res) : "m" (*c) : "%xmm0");
        return res;
}

static inline bool lcSet(lockCounts_t *c, lockCounts_t old, lockCounts_t new)
{
        // This requires GCC be passed '-mcx16'
        return __sync_bool_compare_and_swap(c, old, new);
}

//////////////////////////////////////////////////////////////////
// Constructors
//

static inline lockCounts_t lcActive(gslLockMode_t mode)
{
        return (__uint128_t)1 << (LC_BITS_PER_MODE*mode);
}

static inline lockCounts_t lcSetWait(lockCounts_t c, gslLockMode_t mode, bool wait)
{
        lockCounts_t bit = LC_WAIT_MASK << (LC_BITS_PER_MODE*mode);
        if (wait)
                c |= bit;
        else
                c &= ~bit;
        return c;
}

//////////////////////////////////////////////////////////////////
// Arithmetic
//

static inline lockCounts_t lcAdd(lockCounts_t a, lockCounts_t b)
{
        return a + b;
}

static inline lockCounts_t lcSubtract(lockCounts_t a, lockCounts_t b)
{
        return a - b;
}

//////////////////////////////////////////////////////////////////
// Decomposition
//

static inline bool lcNonZero(lockCounts_t c)
{
        // "return c" works for standard C99 bool, but if bool is just
        // an int, this will simply return the lower n bits.
        return !!c;
}

static inline bool lcZero(lockCounts_t c)
{
        return !c;
}

static inline int lcGetActive(lockCounts_t c, gslLockMode_t mode)
{
        return (c >> (LC_BITS_PER_MODE * mode)) & LC_ACTIVE_MASK;
}

//////////////////////////////////////////////////////////////////
// Masks
//

static inline lockCounts_t lcConflictMask(conflictTable_t *conflicts, gslLockMode_t mode, bool active, bool waiting)
{
        return conflicts->bits[!!active][!!waiting][mode];
}

static inline lockCounts_t lcAnd(lockCounts_t a, lockCounts_t b)
{
        return a & b;
}

static inline lockCounts_t lcOr(lockCounts_t a, lockCounts_t b)
{
        return a | b;
}

static inline lockCounts_t lcMaskSet(void)
{
        return ~0;
}
