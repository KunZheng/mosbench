MEMCACHED_DIR := $(shell cd memcached-1.4.5; pwd)
MY_DIR := $(shell pwd)

all: setup
	$(MAKE) -C build/memcached-stock
	$(MAKE) -C build/memcached-stock install

.PHONY: setup
setup:
	if [ ! -e build/memcached-stock/Makefile ]; then \
		mkdir -p build/memcached-stock && \
		cd build/memcached-stock && \
		$(MEMCACHED_DIR)/configure --prefix=$(MY_DIR)/memcached-stock; \
	fi

clean:
	rm -rf build memcached-stock
