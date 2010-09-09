#ifndef MODES_RWSEM_H
#define MODES_RWSEM_H

#define HAVE_GSL_MODES

typedef enum {
        SHARED,
        EXCLUSIVE,
        GSL_NUM_MODES
} gslLockMode_t;

int conflicts[NUM_MODES] = {
        [SHARED] (1<<EXCLUSIVE),
        [EXCLUSIVE] (1<<SHARED)|(1<<EXCLUSIVE),
};

#endif
