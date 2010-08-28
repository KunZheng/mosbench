BASE47=db-4.7.25.NC
INSTALL47=$(shell pwd)/db4.7-mod

all: db4.7-mod

clean: clean4.7


.PHONY: unpack4.7
unpack4.7: $(BASE47)

$(BASE47):
	tar xzf $(BASE47).tar.gz

patch4.7: $(BASE47)/.stamp-patch

$(BASE47)/.stamp-patch: | $(BASE47)
	patch -p0 -d$(BASE47) < patch.4.7.25.1
	patch -p0 -d$(BASE47) < patch.4.7.25.2
	patch -p0 -d$(BASE47) < patch.4.7.25.3
	patch -p0 -d$(BASE47) < patch.4.7.25.4
	patch -p1 -d$(BASE47) < patch.4.7.25.mosbench
	touch $@

.PHONY: configure4.7
configure4.7: $(BASE47)/.stamp-configure

# (dist/configure replaces the -O CFLAG with what it really wants)
$(BASE47)/.stamp-configure: $(BASE47)/.stamp-patch
	cd $(BASE47)/build_unix && \
	  CFLAGS="-O -DMOSBENCH_CACHE_NPROC -DMOSBENCH_FORCE_MULTICORE" \
	  ../dist/configure --prefix=$(INSTALL47)
	touch $@

.PHONY: build4.7
build4.7: $(BASE47)/.stamp-configure
	$(MAKE) -C $(BASE47)/build_unix

.PHONY: db4.7-mod
db4.7-mod: build4.7
	$(MAKE) -C $(BASE47)/build_unix install

.PHONY: clean4.7
clean4.7:
	$(MAKE) -C $(BASE47)/build_unix clean
