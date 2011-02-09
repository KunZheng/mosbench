#!/usr/bin/python
import subprocess
import sys
import re
import os

START_CORE    = 0
STOP_CORE     = 0
BENCHMARK     = None
DURATION      = 2

BASE_FILENAME = '/root/tmp/foo'

class FopsDir:
    def __init__(self):
        pass

    def run(self, ncores, duration = DURATION):
        p = subprocess.Popen(["o/fops-dir", str(duration), str(ncores), 
                              BASE_FILENAME, '0'],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('FopsDir.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        m = re.search('rate: (\d+\.\d+) per sec', l)
        return float(m.group(1))

    def get_max_base(self):
        return self.run(1)

    def get_min_base(self):
        return self.run(2) / float(2)

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
        'fops-dir':    FopsDir
    }

    global BENCHMARK
    BENCHMARK = benchmarks[argv[1]]()

def main(argv=None):
    if argv is None:
        argv = sys.argv
    if len(argv) < 2:
        usage(argv)

    parse_args(argv)

    minBase = BENCHMARK.get_min_base()
    maxBase = BENCHMARK.get_max_base()

    print '# %s %s %s %s %s' % os.uname()
    print '# min base = %f max base = %f' % (minBase, maxBase)
    print '# cpu\t\tthroughput\tmin scale\tmax scale'
    for c in range(1, STOP_CORE + 1):
        tp = BENCHMARK.run(c)
        minScale = tp / maxBase
        maxScale = tp / minBase
        print '%u\t%f\t%f\t%f' % (c, tp, minScale, maxScale)

if __name__ == '__main__':
    sys.exit(main())
