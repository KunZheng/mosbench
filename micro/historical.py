#!/usr/bin/python
import subprocess
import results
import pickle
import micros
import errno
import time
import sys
import re
import os

START_CORE    = 0
STOP_CORE     = 0
DURATION      = 2
DELAY         = 0
NUM_RUNS      = 3
DEBUG         = False
PICKLE        = 'historical-results/pickle'
VERSIONS      = [ 37, 36, 35, 34, 33, 32, 30, 29, 28, 27, 26, 25 ]
#VERSIONS      = [ 37, 36 ]
BENCHMARKS    = [ micros.Memclone(), micros.Populate() ]

def usage(argv):
    print '''Usage: %s benchmark-name [ -start start -stop stop -duration duration 
  -delay delay -debug debug] 
    'start' is starting core count
    'stop' is ending core count
    'duration' is the duration of each run
    'delay' is the number of seconds to delay before starting
    'debug' is True to enable debugging
''' % argv[0],
    exit(1)

def parse_args(argv):
    args = argv[1:]

    def start_handler(core):
        global START_CORE
        START_CORE = int(core)

    def stop_handler(core):
        global STOP_CORE
        STOP_CORE = int(core)

    def duration_handler(duration):
        global DURATION
        DURATION = int(duration)

    def delay_handler(delay):
        global DELAY
        DELAY = int(delay)

    def debug_handler(debug):
        global DEBUG
        DEBUG = bool(debug)

    handler = {
        '-start'     : start_handler,
        '-stop'      : stop_handler,
        '-duration'  : duration_handler,
        '-delay'     : delay_handler,
        '-debug'     : debug_handler
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

def setup(argv):
    pass

def reboot(name):
    p = subprocess.Popen(['sudo', 'greboot', name],
                         stdin=subprocess.PIPE)
    p.stdin.write('y\n')
    p.stdin.write('y\n')
    p.stdin.write('y\n')
    p.wait()
    if p.returncode:
        raise Exception('grebooot failed: %u' % p.returncode)
    print 'Rebooting...'
    exit(0)

def reboot_default():
    p = subprocess.Popen(['sudo', 'shutdown', '-r', 'now'])
    p.wait()
    if p.returncode:
        raise Exception('shutdown failed: %u' % p.returncode)
    print 'Rebooting into default...'
    exit(0)

def find_kernel_index(version):
    i = 0
    for v in VERSIONS:
        if v == version:
            return i
        i += 1
    return None

def bench_kernel(version):
    if find_kernel_index(version) != None:
        return True
    return False

def next_kernel(version):
    i = find_kernel_index(version)
    if i != None and len(VERSIONS) > (i + 1):
        return VERSIONS[i + 1]
    return None

def num_kernel_remain(version):
    i = find_kernel_index(version)
    return len(VERSIONS) - i

def fixup_motd(version):
    msg = '''
PLEASE LOGOUT

Silas is running experiments on old kernels: %s.

This involves frequent reboots.

This round of experiments should complete in about %u minutes.

''' % (VERSIONS.__str__(), 5 * num_kernel_remain(version) * len(BENCHMARKS))
    p = subprocess.Popen(['sudo', 'sh', '-c', 'echo -ne "%s" > /etc/motd' % msg])
    p.wait()

def do_one(benchmark, dataLog, version, benchResults):
    dataLog.write('# %s\n' % benchmark.get_name())
    dataLog.write('# %s %s %s %s %s\n' % os.uname())
    dataLog.write('# cpu\t\tthroughput\tmin scale\n')

    maxBase = 0
    maxTp = (0, 0)
    maxScale = (0, 0)
    for c in range(1, STOP_CORE + 1):
        tp = 0
        for x in range(0, NUM_RUNS):
            t = benchmark.run(c, DURATION)
            if t > tp:
                tp = t

        if c == 1:
            maxBase = tp
        scale = tp / maxBase
        dataLog.write('%u\t%f\t%f\n' % (c, tp, scale))
        if tp > maxTp[1]:
            maxTp = (c, tp)
        if scale > maxScale[1]:
            maxScale = (c, scale)
        print 'historical: %s %u / %u -- %f %f\r\n' % (benchmark.get_name(), 
                                                       c, STOP_CORE, tp, scale),

    benchResults.get_table('max-throughput').add_row([version, maxTp[0], maxTp[1]])
    benchResults.get_table('max-scale').add_row([version, maxScale[0], maxScale[1]])

    dataLog.write('\n')
    dataLog.write('# %s-max-tp\n' % benchmark.get_name())    
    dataLog.write('%u\t%f\n' % maxTp)    

    dataLog.write('\n')
    dataLog.write('# %s-max-scale\n' % benchmark.get_name())    
    dataLog.write('%u\t%f\n' % maxScale)    

def resume(force = False):
    name = os.uname()[2]
    allResults = None

    if not force and not name.endswith('historical'):
        print 'Not a historical kernel'
        return

    m = re.search('\d+\.\d+\.(\d+)\-.*', name)
    version = int(m.group(1))

    if not force and not bench_kernel(version):
        print 'Not a bench version'
        return

    if not force:
        fixup_motd(version)
    allResults = results.open_results(PICKLE)

    dataLog = open('historical-results/%s.data' % name, 'w+')
    for benchmark in BENCHMARKS:
        if not benchmark.get_name() in allResults:
            maxTp = results.ResultTable('max-throughput', ['version', 'cores', 'throughput'])
            maxScale = results.ResultTable('max-scale', ['version', 'cores', 'scale'])
            result = results.Results(benchmark.get_name())
            result.add_table(maxTp)
            result.add_table(maxScale)
            allResults[benchmark.get_name()] = result
        do_one(benchmark, dataLog, version, allResults[benchmark.get_name()])
    dataLog.close()

    results.save_results(PICKLE, allResults)
    if not force:
        next = next_kernel(version)
        if next:
            reboot('2.6.%u-sbw-historical' % (next))
        else:
            reboot_default()

def main(argv=None):
    if argv is None:
        argv = sys.argv
    parse_args(argv)    

    if len(argv) > 1:
        setup(argv)

    time.sleep(DELAY)
    resume(force = DEBUG)

if __name__ == '__main__':
    sys.exit(main())
