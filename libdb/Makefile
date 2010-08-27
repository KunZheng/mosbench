BASE=db-4.7.25.NC
INSTALL=$(shell pwd)/db-mod

all: install

.PHONY: unpack
unpack: $(BASE)

$(BASE):
	tar xzf $(BASE).tar.gz

patch: $(BASE)/.stamp-patch

$(BASE)/.stamp-patch: | $(BASE)
	patch -p0 -d$(BASE) < patch.4.7.25.1
	patch -p0 -d$(BASE) < patch.4.7.25.2
	patch -p0 -d$(BASE) < patch.4.7.25.3
	patch -p0 -d$(BASE) < patch.4.7.25.4
	patch -p1 -d$(BASE) < mosbench.patch
	touch $@

.PHONY: configure
configure: $(BASE)/.stamp-configure

# (dist/configure replaces the -O CFLAG with what it really wants)
$(BASE)/.stamp-configure: $(BASE)/.stamp-patch
	cd $(BASE)/build_unix && \
	  CFLAGS="-O -DMOSBENCH_CACHE_NPROC -DMOSBENCH_FORCE_MULTICORE" \
	  ../dist/configure --prefix=$(INSTALL)
	touch $@

.PHONY: build
build: $(BASE)/.stamp-configure
	$(MAKE) -C $(BASE)/build_unix

.PHONY: install
install: build
	$(MAKE) -C $(BASE)/build_unix install

clean:
	$(MAKE) -C $(BASE)/build_unix clean
