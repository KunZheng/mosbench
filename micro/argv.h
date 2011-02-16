#include <stdint.h>

struct args {
	const char **valid_args;
	const char *av0;

	/* Don't forget to adjust the arg_table */
	uint64_t	time;
	uint64_t	ncores;
	uint64_t	nprocs;
	uint64_t	use_threads;
	const char 	*sched_op;
};

unsigned int argv_parse(int ac, char **av, struct args *args, 
			const char **valid_args);
void argv_usage(struct args *args);
void argv_sprint(struct args *args, char *buffer, size_t size);
void argv_print(struct args *args);

#ifdef _ARGV_C

static void set_u64_value(struct args *args, const char *value, uint8_t *ptr);
static int  get_u64_value(struct args *args, char *value, const uint8_t *ptr);
static void set_str_value(struct args *args, const char *value, uint8_t *ptr);
static int  get_str_value(struct args *args, char *value, const uint8_t *ptr);

#define DEFINE_U64_ARG(NAME, DESC)					\
	{ #NAME, DESC ? DESC : "u64", offsetof(struct args, NAME),	\
	  set_u64_value, get_u64_value }
#define DEFINE_STR_ARG(NAME, DESC)					\
	{ #NAME, DESC ? DESC : "string", offsetof(struct args, NAME),	\
	  set_str_value, get_str_value }

static struct arg_table {
	const char *name;
	const char *desc;
	off_t offset;
	void (*set_value)(struct args *args, const char *value, uint8_t *ptr);
	int (*get_value)(struct args *args, char *value, const uint8_t *ptr);
} arg_table[] = {
	DEFINE_U64_ARG(time, NULL),
	DEFINE_U64_ARG(ncores, NULL),
	DEFINE_U64_ARG(nprocs, NULL),
	DEFINE_U64_ARG(use_threads, NULL),
	DEFINE_STR_ARG(sched_op, "<create-proc|create-thread|yield>"),

	{ NULL, NULL, 0, NULL, NULL }
};
#endif /* _ARGV_C */
