#!/usr/bin/python
import subprocess
import micros
import sys
import re
import os

START_CORE    = 0
STOP_CORE     = 0
BENCHMARK     = None
DURATION      = 2
NUM_RUNS      = 3

def usage(argv):
    print '''Usage: %s benchmark-name [ -start start -stop stop -duration duration ] 
    'start' is starting core count
    'stop' is ending core count
    'duration' is the duration of each run
''' % argv[0],
    exit(1)

def parse_args(argv):
    args = argv[2:]

    def start_handler(core):
        global START_CORE
        START_CORE = int(core)

    def stop_handler(core):
        global STOP_CORE
        STOP_CORE = int(core)

    def duration_handler(duration):
        global DURATION
        DURATION = int(duration)

    handler = {
        '-start'     : start_handler,
        '-stop'      : stop_handler,
        '-duration'  : duration_handler
    }

    for i in range(0, len(args), 2):
        handler[args[i]](args[i + 1])

    global STOP_CORE
    if STOP_CORE == 0 and hasattr(os, "sysconf"):
        if os.sysconf_names.has_key("SC_NPROCESSORS_ONLN"):
            x = os.sysconf("SC_NPROCESSORS_ONLN")
            if isinstance(x, int) and x > 0:
                STOP_CORE = x

    if STOP_CORE == 0:
        print "Specify number of CPUs on command line"
        usage()

    benchmarks = {
        'fops-dir':    micros.FopsDir,
        'memclone':    micros.Memclone,
        'mempop':      micros.Mempop,
        'memmap':      micros.Memmap,
        'procy':       micros.Procy,
        'exim':        micros.Exim
    }

    global BENCHMARK
    BENCHMARK = benchmarks[argv[1]]()

def main(argv=None):
    if argv is None:
        argv = sys.argv
    if len(argv) < 2:
        usage(argv)

    parse_args(argv)

    maxBase = 0
    minBase = 0

    print '# %s' % BENCHMARK.get_name()
    print '# %s %s %s %s %s' % os.uname()
    print '# cpu\t\tthroughput\tmin scale\tmax scale'
    for c in range(1, STOP_CORE + 1):
        tp = 0
        for x in range(0, NUM_RUNS):
            t = BENCHMARK.run(c, DURATION)
            if t > tp:
                tp = t

        if c == 1:
            maxBase = tp
            minScale = tp / maxBase
            print '%u\t%f\t%f' % (c, tp, minScale)
            continue
        if c == 2:
            minBase = tp / 2

        minScale = tp / maxBase
        maxScale = tp / minBase
        print '%u\t%f\t%f\t%f' % (c, tp, minScale, maxScale)

if __name__ == '__main__':
    sys.exit(main())
