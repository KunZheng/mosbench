all: all-exim all-postgres all-gmake all-psearchy all-metis

clean: clean-psearchy clean-metis

.PHONY: all-libdb
all-libdb:
	$(MAKE) -C libdb all

.PHONY: clean-libdb
clean-libdb:
	$(MAKE) -C libdb clean

.PHONY: all-exim
all-exim: all-libdb
	$(MAKE) -C exim all

.PHONY: clean-exim
clean-exim:
	$(MAKE) -C exim clean
	$(MAKE) -C exim exim-clean

.PHONY: all-postgres
all-postgres:
	$(MAKE) -C postgres all

.PHONY: clean-postgres
clean-postgres:
	$(MAKE) -C postgres clean

.PHONY: clean-gmake
clean-gmake:

.PHONY: all-gmake
all-gmake:

.PHONY: all-psearchy
all-psearchy:
	$(MAKE) -C psearchy/mkdb all

.PHONY: clean-psearchy
clean-psearchy:
	$(MAKE) -C psearchy/mkdb clean

.PHONY: all-metis
all-metis:
	$(MAKE) -C metis O=obj.default SF_MODEL=default obj.default/app/wrmem obj.default/app/wrmem.sf
	$(MAKE) -C metis O=obj.hugetlb SF_MODEL=hugetlb HUGETLB_MOUNT=/tmp/mosbench/hugetlb obj.hugetlb/app/wrmem obj.hugetlb/app/wrmem.sf

.PHONY: clean-metis
clean-metis:
	$(MAKE) -C metis O=obj.default clean
	$(MAKE) -C metis O=obj.hugetlb clean

.PHONY: bench
bench:
	python config.py
