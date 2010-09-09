PG_CFLAGS = -g -Wall -O2 -fPIC
PG_LDFLAGS = -Wl,--as-needed
PG_CONFIG = --enable-nls --enable-integer-datetimes --enable-thread-safety \
	--enable-debug --enable-depend --disable-rpath \
	--with-gnu-ld --with-pgport=5432

# Always enable lock stats
PG_CFLAGS += -DLWLOCK_STATS -DLWLOCK_TIMING_STATS -fno-omit-frame-pointer

# For debugging
#PG_CFLAGS += -O0
#PG_CONFIG += --enable-cassert

PG_DIR := $(shell cd postgresql-8.3.9; pwd)
MY_DIR := $(shell pwd)

all: pg-sysv pg-sysv-lwscale pg-sysv-lwscale-lockscale pg-posix pg-posix-lwscale pg-posix-lwscale-lockscale
clean: clean-sysv clean-sysv-lwscale clean-sysv-lwscale-lockscale clean-posix clean-posix-lwscale clean-posix-lwscale-lockscale
	rm -rf build

.PHONY: always
pg-%: always
	@$(MAKE) --no-print-directory vars-$*- TARGET=$@

clean-%: always
	rm -rf pg-$*

vars-:
	if [ ! -e $(MY_DIR)/build/$(TARGET)/GNUmakefile ]; then \
		mkdir -p $(MY_DIR)/build/$(TARGET) && \
		cd $(MY_DIR)/build/$(TARGET) && \
		$(PG_DIR)/configure --prefix=$(MY_DIR)/$(TARGET) $(PG_CONFIG) CFLAGS="$(PG_CFLAGS)" LDFLAGS="$(PG_LDFLAGS)"; \
	fi
	$(MAKE) -C $(MY_DIR)/build/$(TARGET)
	$(MAKE) -C $(MY_DIR)/build/$(TARGET) install

vars-sysv-: vars-
	@true

vars-posix-: PG_LDFLAGS += -pthread
vars-posix-: PG_CONFIG += USE_UNNAMED_POSIX_SEMAPHORES=1
vars-posix-: vars-
	@true

vars-%-lwscale-: PG_CFLAGS += -DLWLOCK_SCALABLE=1
vars-%-lwscale-: vars-%-
	@true

vars-%-lockscale-: PG_CFLAGS += -DLOCK_SCALABLE=1
vars-%-lockscale-: vars-%-
	@true
