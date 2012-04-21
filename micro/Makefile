-include ../config.mk
SUPPORT = ../support

# Quiet compile, unset for verbose output
Q	= @

O	= o
CC	= gcc

ifneq ($(QEMUDIR),)
MTRACE	  = -D_MTRACE -I$(QEMUDIR)
MTRACEDEP = $(QEMUDIR)/mtrace-magic.h
endif

CWARNS	= -Wformat=2 -Wextra -Wmissing-noreturn -Wwrite-strings -Wshadow \
	  -Wno-unused-parameter -Wmissing-format-attribute -fno-builtin \
	  -Wswitch-default -Wmissing-prototypes \
	  -Wmissing-declarations
CFLAGS  = $(DEFS) -Wall -Werror -fno-strict-aliasing $(CWARNS) \
	  -g -O3 -D_GNU_SOURCE -m64 $(MTRACE) -I..

LIBS	= bench.c
DEPS	= bench.h config.h list.h $(LIBS) $(MTRACEDEP) $(SUPPORT)/mtrace.h

BINS	= memclone fops-dir proc-ping-pong lock latency mapbench dirbench
BINS	:= $(addprefix $(O)/, $(BINS))

all: $(BINS)

$(O)/%: %.c $(DEPS)
	@mkdir -p $(@D)
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -o $@ $< $(LIBS) -lpthread

clean:
	@echo "CLEAN"
	$(Q)rm -rf $(O)
