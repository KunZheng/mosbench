#!/usr/bin/python
import subprocess
import micros
import sys
import re
import os

START_CORE    = 1
STOP_CORE     = 48
BENCHMARK     = None
DURATION      = 5
NUM_RUNS      = 3
PRINT_SCALE   = True

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
        'fops-dir':    micros.FopsDir(),
        'memclone':    micros.Memclone(),
        'mempop':      micros.Mempop(),
        'memmap':      micros.Memmap(),
        'procy':       micros.Procy(),
        'exim':        micros.Exim(),
        'procy-exec':  micros.ProcyExec(),
        'pagecache':   micros.Pagecache(),
        'bfish-shared-1': micros.BFish(),
        'bfish-shared-2': micros.BFish(nclines=2),
        'bfish-shared-4': micros.BFish(nclines=4),
        'bfish-shared-8': micros.BFish(nclines=8),
        'bfish-shared-16': micros.BFish(nclines=16),
        'bfish-msg-1': micros.BFish(nclines=1, bfishCommand='o/bfish-msg'),
        'bfish-msg-2': micros.BFish(nclines=2, bfishCommand='o/bfish-msg'),
        'bfish-msg-4': micros.BFish(nclines=4, bfishCommand='o/bfish-msg'),
        'bfish-msg-8': micros.BFish(nclines=8, bfishCommand='o/bfish-msg'),
        'bfish-msg-16': micros.BFish(nclines=16, bfishCommand='o/bfish-msg')
    }

    global BENCHMARK
    BENCHMARK = benchmarks[argv[1]]

def best_run(ncores, duration, nruns):
    tp = 0
    for x in range(nruns):
        t = BENCHMARK.run(ncores, duration)
        if t > tp:
            tp = t
    return tp

def main(argv=None):
    if argv is None:
        argv = sys.argv
    if len(argv) < 2:
        usage(argv)

    parse_args(argv)

    minBase = 0
    maxBase = 0
    if PRINT_SCALE and START_CORE > 1:
        minBase = best_run(1, DURATION, NUM_RUNS)
    if PRINT_SCALE and START_CORE > 2:
        maxBase = best_run(2, DURATION, NUM_RUNS) / 2

    print '# %s' % BENCHMARK.get_name()
    print '# %s %s %s %s %s' % os.uname()
    if PRINT_SCALE:
        print '# cpu\t\tthroughput\tmin scale\tmax scale'
    else:
        print '# cpu\t\tthroughput\t'
    sys.stdout.flush()

    for c in range(START_CORE, STOP_CORE + 1):
        tp = best_run(c, DURATION, NUM_RUNS)
        if PRINT_SCALE and c == 1:
            minBase = tp
            minScale = tp / minBase
            print '%u\t%f\t%f' % (c, tp, minScale)
            continue
        if PRINT_SCALE and c == 2:
            maxBase = tp / 2

        if PRINT_SCALE:
            minScale = tp / minBase
            maxScale = tp / maxBase
            print '%u\t%f\t%f\t%f' % (c, tp, minScale, maxScale)
        else:
            print '%u\t%f\t' % (c, tp)
        sys.stdout.flush()

if __name__ == '__main__':
    sys.exit(main())
