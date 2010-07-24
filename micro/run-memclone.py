#!/usr/bin/python
import subprocess
import sys
import re
import os

mbytes          = 100
threading       = 1

def run_one(ncpu):
    p = subprocess.Popen(["o/memclone", str(ncpu), str(mbytes), 
                          str(threading)], stdout=subprocess.PIPE)
    p.wait()
    if p.returncode:
        print "memclone %u failed" % ncpu
        exit(1)

    l = p.stdout.readline().strip()
    m = re.search('ave cycles/pf (\d+), max cycles/pf (\d+)', l)
    print "%u\t\t%s\t\t%s" % (ncpu, m.group(1), m.group(2))

def main(argv=None):
    if argv is None:
        argv = sys.argv

    ncpus = 0;
    if len(argv) > 1:
        ncpus = int(argv[1])
    elif hasattr(os, "sysconf"):
        if os.sysconf_names.has_key("SC_NPROCESSORS_ONLN"):
            x = os.sysconf("SC_NPROCESSORS_ONLN")
            if isinstance(x, int) and x > 0:
                ncpus = x

    if ncpus == 0:
        print "Specify number of CPUs on command line"
        exit(1)

    print "# %s %s %s %s %s" % os.uname()
    print "# mbytes = %u, threading = %u, units = cycles" % (mbytes, threading)
    print "#\t\tave\t\tmax"
    for c in range(1, ncpus + 1):
        run_one(c)

if __name__ == "__main__":
    sys.exit(main())
