# Location of the qemu mtrace header files
QEMUDIR ?= /home/sbw/qemu



ifneq ($(QEMUDIR),)
MTRACE_CFLAGS = -D_MTRACE -I $(QEMUDIR)
else
MTRACE_CFLAGS =
endif
