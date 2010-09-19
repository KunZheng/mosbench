PG_CFLAGS = -Wall -O2 -fPIC -g -fno-omit-frame-pointer
PG_LDFLAGS = -Wl,--as-needed
PG_CONFIG = --enable-nls --enable-integer-datetimes --enable-thread-safety \
	--enable-debug --enable-depend --disable-rpath \
	--with-gnu-ld --with-pgport=5432

# Enable lock stats (warning: this spends a non-trivial amount of time
# in gettimeofday)
#PG_CFLAGS += -DLWLOCK_STATS -DLWLOCK_TIMING_STATS

# For debugging
#PG_CFLAGS += -O0
#PG_CONFIG += --enable-cassert

PG_DIR := $(shell cd postgresql-8.3.9; pwd)
MY_DIR := $(shell pwd)

VARIANTS := sysv sysv-lwscale sysv-lwscale-lockscale posix posix-lwscale posix-lwscale-lockscale

all: $(addprefix pg-,$(VARIANTS))

clean: $(addprefix clean-,$(VARIANTS))
	rm -rf build
	$(MAKE) -C pgload clean

.PHONY: always

pg-%: always
	@$(MAKE) --no-print-directory setup-$*- TARGET=$@
	$(MAKE) -C $(MY_DIR)/build/pg-$*
	$(MAKE) -C $(MY_DIR)/build/pg-$* install

clean-%: always
	rm -rf pg-$*

setup-:
	if [ ! -e $(MY_DIR)/build/$(TARGET)/GNUmakefile ]; then \
		mkdir -p $(MY_DIR)/build/$(TARGET) && \
		cd $(MY_DIR)/build/$(TARGET) && \
		$(PG_DIR)/configure --prefix=$(MY_DIR)/$(TARGET) $(PG_CONFIG) CFLAGS="$(PG_CFLAGS)" LDFLAGS="$(PG_LDFLAGS)"; \
	fi

setup-sysv-: setup-
	@true

setup-posix-: PG_LDFLAGS += -pthread
setup-posix-: PG_CONFIG += USE_UNNAMED_POSIX_SEMAPHORES=1
setup-posix-: setup-
	@true

setup-%-lwscale-: PG_CFLAGS += -DLWLOCK_SCALABLE=1
setup-%-lwscale-: setup-%-
	@true

setup-%-lockscale-: PG_CFLAGS += -DLOCK_SCALABLE=1
setup-%-lockscale-: setup-%-
	@true
