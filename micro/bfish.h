static inline void do_op0(char *ptr)
{
	__asm__ __volatile__("lock; incq %0" : "+m" (*ptr));
}

static inline void do_op1(char *ptr)
{
	__asm__ __volatile__("incq %0" : "+m" (*ptr));
}

#define XOP do_op1
