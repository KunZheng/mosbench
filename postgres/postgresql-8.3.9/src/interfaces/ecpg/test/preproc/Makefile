subdir = src/interfaces/ecpg/test/preproc
top_builddir = ../../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/$(subdir)/../Makefile.regress


TESTS = array_of_struct array_of_struct.c \
	autoprep autoprep.c \
	comment comment.c \
	define define.c \
	init init.c \
	type type.c \
	variable variable.c \
	whenever whenever.c

all: $(TESTS)

array_of_struct.c:	array_of_struct.pgc ../regression.h
	$(ECPG) -c -o $@ -I$(srcdir) $<

autoprep.c:     autoprep.pgc ../regression.h
	$(ECPG) -r prepare -o $@ -I$(srcdir) $<

