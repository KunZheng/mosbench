CONFIG := --with-included-apr --with-mpm=worker \
	  --enable-nonportable-atomics=yes --disable-log-config \
	  --enable-file-cache --enable-cache --enable-disk-cache \
	  --enable-mem-cache

# NO_LINGCLOSE - Do not wait for the client to acknowledge the socket
# close.  This violates HTTP recommendations, but saves us a
# round-trip to the clients, which has a huge impact on how much load
# we can take.
#
# MOSBENCH_PIN - Pin each Apache process to a core, in sequential
# order.
#
# MOSBENCH_MULTI_ACCEPT - Use multi-accept.  This requires
# MOSBENCH_PIN and that the number of processes in the Apache instance
# exactly equal the number of cores.
APACHE_DEFINES := -DNO_LINGCLOSE \
	-DMOSBENCH_PIN \
	-DMOSBENCH_MULTI_ACCEPT

APACHE_DIR := $(shell cd httpd-2.2.14; pwd)
MY_DIR := $(shell pwd)

all: setup
	$(MAKE) -C build/apache-mod
	$(MAKE) -C build/apache-mod install

# Apache ALMOST builds out-of-tree.  The two symlinks fix tools it
# assumes are in the source directory, but actually wind up in the
# build directory.
#
# Also, Apache normally builds with -O2, but overriding the CFLAGS
# disables the logic that adds that, so we pass it explicitly.
.PHONY: setup
setup:
	if [ ! -e build/apache-mod/Makefile ]; then \
		mkdir -p build/apache-mod && \
		cd build/apache-mod && \
		ln -sf $(MY_DIR)/build/apache-mod/srclib/apr/apr-1-config $(APACHE_DIR)/srclib/apr/apr-1-config && \
		ln -sf $(MY_DIR)/build/apache-mod/srclib/apr-util/apu-1-config $(APACHE_DIR)/srclib/apr-util/apu-1-config && \
		CFLAGS="$(APACHE_DEFINES) -O2" $(APACHE_DIR)/configure $(CONFIG) --prefix=$(MY_DIR)/apache-mod; \
	fi

clean:
	rm -rf build apache-mod $(APACHE_DIR)/srclib/apr/apr-1-config $(APACHE_DIR)/srclib/apr-util/apu-1-config
