TOPDIR := ..

PGCONFIG := pg_config

CFLAGS := -std=c99 -I`$(PGCONFIG) --includedir`
LDFLAGS := -L`$(PGCONFIG) --libdir`

pgload: pgload.c $(TOPDIR)/libload/libload.o
	gcc -o $@ -I$(TOPDIR) $(CFLAGS) $^ $(LDFLAGS) -lpq
#	gcc -static -o $@ -I$(TOPDIR) $(CFLAGS) $^ $(LDFLAGS) -lpq  -lpthread -lcrypt

clean: clean-pgload

clean-pgload:
	rm -f pgload

include $(TOPDIR)/libload/Makefrag
