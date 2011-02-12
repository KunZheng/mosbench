#define __xg(x) ((volatile long *)(x))

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
#define __raw_cmpxchg(ptr, old, new, size, lock)			\
({									\
	__typeof__(*(ptr)) __ret;					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	switch (size) {							\
	case 1:								\
		asm volatile(lock "cmpxchgb %b1,%2"			\
			     : "=a"(__ret)				\
			     : "q"(__new), "m"(*__xg(ptr)), "0"(__old)	\
			     : "memory");				\
		break;							\
	case 2:								\
		asm volatile(lock "cmpxchgw %w1,%2"			\
			     : "=a"(__ret)				\
			     : "r"(__new), "m"(*__xg(ptr)), "0"(__old)	\
			     : "memory");				\
		break;							\
	case 4:								\
		asm volatile(lock "cmpxchgl %k1,%2"			\
			     : "=a"(__ret)				\
			     : "r"(__new), "m"(*__xg(ptr)), "0"(__old)	\
			     : "memory");				\
		break;							\
	case 8:								\
		asm volatile(lock "cmpxchgq %1,%2"			\
			     : "=a"(__ret)				\
			     : "r"(__new), "m"(*__xg(ptr)), "0"(__old)	\
			     : "memory");				\
		break;							\
	default:							\
		die("__raw_cmpxchg: wrong_size");			\
	}								\
	__ret;								\
})

#define __cmpxchg(ptr, old, new, size)					\
	__raw_cmpxchg((ptr), (old), (new), (size), LOCK_PREFIX)

#define __sync_cmpxchg(ptr, old, new, size)				\
	__raw_cmpxchg((ptr), (old), (new), (size), "lock; ")

#define __cmpxchg_local(ptr, old, new, size)				\
	__raw_cmpxchg((ptr), (old), (new), (size), "")

#define cmpxchg(ptr, old, new)						\
	__cmpxchg((ptr), (old), (new), sizeof(*ptr))

#define sync_cmpxchg(ptr, old, new)					\
	__sync_cmpxchg((ptr), (old), (new), sizeof(*ptr))

#define cmpxchg_local(ptr, old, new)					\
	__cmpxchg_local((ptr), (old), (new), sizeof(*ptr))

#define cmpxchg64(ptr, o, n)						\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	cmpxchg((ptr), (o), (n));					\
})

#define cmpxchg64_local(ptr, o, n)					\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	cmpxchg_local((ptr), (o), (n));					\
})
