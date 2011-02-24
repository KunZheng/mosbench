static inline void do_op0(char *ptr)
{
	__asm__ __volatile__("lock; incq %0" : "+m" (*ptr));
}

static inline void do_op1(char *ptr)
{
	__asm__ __volatile__("incq %0" : "+m" (*ptr));
}

static inline void do_op2(char *ptr)
{
	volatile uint64_t *p = (volatile uint64_t *)ptr;
	*p = 0;
}

#define XOP do_op2
