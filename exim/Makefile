# Quiet compile, unset for verbose output
Q	= @
OS 	= $(shell uname)

CC	= gcc

CWARNS	= -Wformat=2 -Wextra -Wmissing-noreturn -Wwrite-strings -Wshadow \
	  -Wno-unused-parameter -Wmissing-format-attribute -fno-builtin \
	  -Wswitch-default -Wmissing-prototypes \
	  -Wmissing-declarations
CFLAGS  = $(DEFS) -Wall -Werror -fno-strict-aliasing $(CWARNS) \
	  -g -O3

ifeq ($(OS), Linux)
LIBS = 
endif
ifeq ($(OS), SunOS)
LIBS = -lsocket -lnsl
endif

all: smtpbm

smtpbm: smtpbm.c
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -o $@ $< $(LIBS)
clean:
	@echo "CLEAN"
	$(Q)rm -rf smtpbm
