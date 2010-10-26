# Location of the qemu mtrace header files
QEMUDIR = /home/sbw/qemu

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
	  -g -O3 -D_GNU_SOURCE -m64 $(MTRACE)

LIBS	= bench.c
DEPS	= bench.h mtrace.h config.h $(LIBS) $(MTRACEDEP)

BINS	= memclone fops-dir
BINS	:= $(addprefix $(O)/, $(BINS))

all: $(BINS)

$(O)/%: %.c $(DEPS)
	@mkdir -p $(@D)
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -o $@ $< $(LIBS) -lpthread

clean:
	@echo "CLEAN"
	$(Q)rm -rf $(O)
