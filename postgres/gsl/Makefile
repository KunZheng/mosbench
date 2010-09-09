CL_SRCS=checklock.c gsl-parex.c parex.c

checklock: ${CL_SRCS} *.h
	gcc -o $@ -g -O2 -mcx16 -std=gnu99 -lpth ${CL_SRCS}

.PHONY: gdb
gdb: checklock
	gdb $<
