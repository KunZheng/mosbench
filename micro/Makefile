# Quiet compile, unset for verbose output
Q	= @
O	= o

CC	= gcc

CWARNS	= -Wformat=2 -Wextra -Wmissing-noreturn -Wwrite-strings -Wshadow \
	  -Wno-unused-parameter -Wmissing-format-attribute -fno-builtin \
	  -Wswitch-default -Wmissing-prototypes \
	  -Wmissing-declarations
CFLAGS  = $(DEFS) -Wall -Werror -fno-strict-aliasing $(CWARNS) \
	  -g -O3 -D_GNU_SOURCE -m64

LIBS	= bench.c
DEPS	= bench.h $(LIBS) 

BINS	= memclone
BINS	:= $(addprefix $(O)/, $(BINS))

all: $(BINS)

$(O)/%: %.c $(DEPS)
	@mkdir -p $(@D)
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -o $@ $< $(LIBS) -lpthread

clean:
	@echo "CLEAN"
	$(Q)rm -rf $(O)
