#include <stdint.h>

struct args {
	const char **valid_args;
	const char *av0;
	
	uint64_t	time;
	uint64_t	ncores;
	uint64_t	use_threads;
	const char 	*sched_op;
};

unsigned int argv_parse(int ac, char **av, struct args *args, const char **valid_args);
void argv_usage(struct args *args);
void argv_sprint(struct args *args, char *buffer, size_t size);
void argv_print(struct args *args);
