#!/bin/sh

cmds='run-cmdline="/root/fops-dir 1 2 /root/tmp/foo 1" '
cmds=$cmds'run-cmdline="/root/proc-ping-pong 0 1 1" '
cmds=$cmds'run-cmdline="shutdown -h now"'

#qemu-system-x86_64							\

/home/sbw/qemu/local/bin/qemu-system-x86_64				\
     -smp 2								\
     -m 256								\
     -kernel obj.qemu/arch/x86_64/boot/bzImage				\
     -hda ../emu/disk2.img						\
     -append "root=/dev/hda console=ttyS0 $cmds"  			\
     -nographic								\
     -redir tcp:9922::22 						\
     -mtrace-file /tmp/mtrace.out -mtrace-format binary

#     -serial stdio							\
