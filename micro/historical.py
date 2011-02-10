#!/usr/bin/python
import subprocess
import time
import sys
import re
import os

START_CORE    = 0
STOP_CORE     = 0
BENCHMARK     = None
DURATION      = 2
DATA_FILE     = None
DELAY         = 0
NUM_RUNS      = 3

START_VERSION  = 37
FINAL_VERSION  = 37

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

    def get_name(self):
        return 'fops-dir'

def usage(argv):
    print '''Usage: %s benchmark-name [ -start start -stop stop -duration duration 
  -delay delay] 
    'start' is starting core count
    'stop' is ending core count
    'duration' is the duration of each run
    'delay' is the number of seconds to delay before starting
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

    handler = {
        '-start'     : start_handler,
        '-stop'      : stop_handler,
        '-duration'  : duration_handler,
        '-delay'     : delay_handler
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

    global BENCHMARK
    BENCHMARK = FopsDir()

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

def bench_kernel(version):
    pass

def next_kernel(version):
    pass

def fixup_motd(version):
    msg = '''
PLEASE LOGOUT

Silas is running experiments on kernels %u to %u.

This involves frequent reboots.

This round of experiments should complete in about %u minutes.

''' % (START_VERSION, FINAL_VERSION, 5 * (1 + version - FINAL_VERSION))
    p = subprocess.Popen(['sudo', 'sh', '-c', 'echo -ne "%s" > /etc/motd' % msg])
    p.wait()

def resume(force = False):
    name = os.uname()[2]

    if not force and not name.endswith('historical'):
        print 'Not a historical kernel'
        return

    m = re.search('\d+\.\d+\.(\d+)\-.*', name)
    version = int(m.group(1))

    if version < FINAL_VERSION or version > START_VERSION:
        return

    fixup_motd(version)

    DATA_FILE = open('historical-results/%s.data' % name, 'w+')
    DATA_FILE.write('# %s\n' % BENCHMARK.get_name())
    DATA_FILE.write('# %s %s %s %s %s\n' % os.uname())
    DATA_FILE.write('# cpu\t\tthroughput\tmin scale\n')

    maxBase = 0
    maxTp = (0, 0)
    maxScale = (0, 0)
    for c in range(1, STOP_CORE + 1):
        tp = 0
        for x in range(0, NUM_RUNS):
            t = BENCHMARK.run(c)
            if t > tp:
                tp = t

        if c == 1:
            maxBase = tp
        scale = tp / maxBase
        DATA_FILE.write('%u\t%f\t%f\n' % (c, tp, scale))
        if tp > maxTp[1]:
            maxTp = (c, tp)
        if scale > maxScale[1]:
            maxScale = (c, scale)
        print 'historical: %u / %u -- %f %f\r\n' % (c, STOP_CORE, tp, scale),

    DATA_FILE.write('\n')
    DATA_FILE.write('# %s-max-tp\n' % BENCHMARK.get_name())    
    DATA_FILE.write('%u\t%f\n' % maxTp)    

    DATA_FILE.write('\n')
    DATA_FILE.write('# %s-max-scale\n' % BENCHMARK.get_name())    
    DATA_FILE.write('%u\t%f\n' % maxScale)    
    DATA_FILE.close()

    if version != FINAL_VERSION:
        reboot('2.6.%u-sbw-historical' % (version - 1))
    else:
        reboot_default()

def main(argv=None):
    if argv is None:
        argv = sys.argv
    parse_args(argv)    

    if len(argv) > 1:
        setup(argv)

    time.sleep(DELAY)
    resume()

if __name__ == '__main__':
    sys.exit(main())
