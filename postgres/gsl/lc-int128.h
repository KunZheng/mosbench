#ifndef LC_INT128_H
#define LC_INT128_H

#define LC_MAX_MODES     8
typedef int gslLockMode_t;

typedef __uint128_t lockCounts_t;
typedef struct
{
        __uint128_t bits[2][2][LC_MAX_MODES];
} conflictTable_t;

void lcInitConflictTable(conflictTable_t *m, int *conflicts, int nConflicts);

#endif
