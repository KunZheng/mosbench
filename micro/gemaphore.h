#include "atomic.h"

struct gemaphore {
	atomic_t count;
	unsigned int goal;
};

static inline void gemaphore_v(struct gemaphore *g)
{
	atomic_inc(&g->count);
}

static inline void gemaphore_p(struct gemaphore *g)
{
	while (atomic_read(&g->count) != g->goal)
		nop_pause();
}

static inline void gemaphore_init(struct gemaphore *g, unsigned int goal)
{
	atomic_set(&g->count, 0);
	g->goal = goal;
}
