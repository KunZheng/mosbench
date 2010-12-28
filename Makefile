TARGETS=exim memcached apache postgres gmake psearchy metis micro

all: $(addprefix all-,$(TARGETS))
clean: $(addprefix clean-,$(TARGETS))

.PHONY: always

all-libdb: always
	$(MAKE) -C libdb all

clean-libdb: always
	$(MAKE) -C libdb clean

all-exim: all-libdb always
	$(MAKE) -C exim all

clean-exim: always
	$(MAKE) -C exim clean
	$(MAKE) -C exim exim-clean

all-memcached: always
	$(MAKE) -C memcached all

clean-memcached: always
	$(MAKE) -C memcached clean

all-apache: always
	$(MAKE) -C apache all

clean-apache: always
	$(MAKE) -C apache clean

all-postgres: always
	$(MAKE) -C postgres all

clean-postgres: always
	$(MAKE) -C postgres clean

clean-gmake: always

all-gmake: always

all-psearchy: always
	$(MAKE) -C psearchy/mkdb all

clean-psearchy: always
	$(MAKE) -C psearchy/mkdb clean

all-metis: always
	$(MAKE) -C metis O=obj.default SF_MODEL=default obj.default/app/wrmem obj.default/app/wrmem.sf
	$(MAKE) -C metis O=obj.hugetlb SF_MODEL=hugetlb HUGETLB_MOUNT=/tmp/mosbench/hugetlb obj.hugetlb/app/wrmem obj.hugetlb/app/wrmem.sf

clean-metis: always
	$(MAKE) -C metis O=obj.default clean
	$(MAKE) -C metis O=obj.hugetlb clean

all-micro: always
	$(MAKE) -C micro all

clean-micro: always
	$(MAKE) -C micro clean

.PHONY: bench
bench:
	python config.py
