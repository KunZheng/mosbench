BASE=db-4.7.25.NC
INSTALL=$(shell pwd)/db-mod

all: install

.PHONY: unpack
unpack: .stamp.unpack

.stamp.unpack:
	tar xzf $(BASE).tar.gz
	patch -p0 -d$(BASE) < patch.4.7.25.1
	patch -p0 -d$(BASE) < patch.4.7.25.2
	patch -p0 -d$(BASE) < patch.4.7.25.3
	patch -p0 -d$(BASE) < patch.4.7.25.4
	touch $@

.PHONY: configure
configure: .stamp.configure

.stamp.configure: .stamp.unpack
	cd $(BASE)/build_unix && ../dist/configure --prefix=$(INSTALL)
	touch $@

.PHONY: build
build: .stamp.configure
	$(MAKE) -C $(BASE)/build_unix

.PHONY: install
install: build
	$(MAKE) -C $(BASE)/build_unix install

clean:
	$(MAKE) -C $(BASE)/build_unix clean
