#!/usr/bin/python

import shlex
import subprocess
import os
import re
import sys
import time

for arg in sys.argv[1:]: 
    time.sleep(int(arg));

f = open('/proc/cmdline', 'r')
cmdline = f.readline()

#cmdline = 'root=/dev/hda console=ttyS0 run-cmdline="/bin/ls" lkjdsa run-cmdline="goof ball"';

for m in re.finditer('run-cmdline="(.*?)"\s*', cmdline):
    cmd = m.group(1)
    args = shlex.split(cmd)

    try:
        print 'Running %s ...' % cmd
        p = subprocess.Popen(args)
        p.wait()
        if p.returncode:
            print 'Command "%s" failed: %d' % (cmd, p.returncode)
    except OSError, e:
        print 'Failed to run command "%s" failed: %s' % (cmd, os.strerror(e.errno))
