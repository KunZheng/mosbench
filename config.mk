# Location of the qemu mtrace header files
QEMUDIR ?=



ifneq ($(QEMUDIR),)
MTRACE_CFLAGS = -D_MTRACE -I $(QEMUDIR)
else
MTRACE_CFLAGS =
endif
