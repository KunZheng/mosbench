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

# Build Exim with our tweaked libdb
DBMLIB  = $(abspath ../libdb/db-mod/lib/libdb.a)

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


.PHONY: exim-mod
exim-mod:
	mkdir -p exim-4.71/Local
	sed -e "s,@INSTALL_DIR@,`pwd`/$@,;s,@USER@,$$USER,;s,@DBMLIB@,$(DBMLIB)," < Local-Makefile.tmpl > exim-4.71/Local/Makefile.tmp
	if ! cmp exim-4.71/Local/{Makefile,Makefile.tmp}; then \
	  $(MAKE) -C exim-4.71 distclean; \
	  mv exim-4.71/Local/Makefile.tmp exim-4.71/Local/Makefile; \
	fi
	$(MAKE) -C exim-4.71
	$(MAKE) -C exim-4.71 install INSTALL_ARG=-no_chown

.PHONY: exim-clean
exim-clean:
	$(MAKE) -C exim-4.71 distclean
	rm -rf exim-mod exim-4.71/Local
