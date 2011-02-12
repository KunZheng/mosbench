#!/usr/bin/python
import subprocess
import results
import pickle
import micros
import errno
import copy
import time
import sys
import re
import os

# Config
BENCHMARK     = micros.Populate()
MIN_TPUT      = 4000000.0

# Other knobs
COMMAND_LINE  = 'root=/dev/sda2 ro console=tty0 console=ttyS1,19200n8r quiet'
START_CORE    = 1
STOP_CORE     = 15
DURATION      = 5
NUM_RUNS      = 3
DELAY         = 0
DEBUG         = False

def usage(argv):
    print '''Usage: %s benchmark-name [ -start start -stop stop -duration duration]
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

    def debug_handler(debug):
        global DEBUG
        DEBUG = bool(debug)

    def delay_handler(delay):
        global DELAY
        DELAY = int(delay)

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

def fixup_motd():
    msg = '''
PLEASE LOGOUT

Silas is running git-bisect to find when a scalability bug was introduced.

This involves frequent reboots.

This round of searching should complete shortly.  See '# screen -x tom' on
hooverdam for more information.

'''
    p = subprocess.Popen(['sudo', 'sh', '-c', 'echo -ne "%s" > /etc/motd' % msg])
    p.wait()

def bad_kernel_exit():
    exit(1)

def good_kernel_exit():
    exit(0)

def test_kernel():
    maxBase = 0
    maxTp = (0, 0)
    maxScale = (0, 0)
    for c in range(START_CORE, STOP_CORE + 1):
        tp = 0
        for x in range(0, NUM_RUNS):
            t = BENCHMARK.run(c, DURATION)
            if t > tp:
                tp = t

        if c == 1:
            maxBase = tp
        scale = tp / maxBase
        if tp > maxTp[1]:
            maxTp = (c, tp)
        if scale > maxScale[1]:
            maxScale = (c, scale)
        print 'bisect: %s %u / %u -- %f %f (max scale %u, %f) (max tp %u, %f)\r\n' % (BENCHMARK.get_name(), 
                                                                                      c, STOP_CORE, tp, scale,
                                                                                      maxScale[0], maxScale[1], 
                                                                                      maxTp[0], maxTp[1]),
        if maxTp[1] > MIN_TPUT:
            print 'good kernel (min tp %f)\r\n' % MIN_TPUT,
            return True
    
    print 'bad kernel (min tp %f)\r\n' % MIN_TPUT,
    return False

class BisectHelper(object):
    def __init__(self, kernelPath, objPath, gitLog, buildLog):
        self.kernelPath = kernelPath
        self.objPath = objPath
        self.gitLog = gitLog
        self.buildLog = buildLog

    def __git_bisect(self, command):
        p = subprocess.Popen(['git', 'bisect', command],
                             cwd=self.kernelPath,
                             stdout=subprocess.PIPE)
        done = False
        for l in p.stdout:
            self.gitLog.write(l)
            self.gitLog.flush()
            if l.endswith('is the first bad commit'):
                done = True
        p.wait()
        if p.returncode:
            raise Exception('git bisect %s failed: %u' % (command, p.returncode))
        return done
        
    def good(self):
        return self.__git_bisect('good')

    def bad(self):
        return self.__git_bisect('bad')

    def bisectLog(self):
        self.__git_bisect('log')

    def config(self):
        env = copy.copy(os.environ)
        env['LOCAL_VERSION'] = ''
        p = subprocess.Popen(['sh', 'gen-config.sh', self.objPath],
                             cwd=self.kernelPath, env=env,
                             stdout=self.buildLog,
                             stderr=self.buildLog)
        p.wait()
        if p.returncode:
            raise Exception('gen-config.sh failed: %u' % p.returncode)
        
    
    def build(self):
        self.config()

        env = copy.copy(os.environ)
        env['LOCAL_VERSION'] = ''
        p = subprocess.Popen(['make', 'O=%s' % self.objPath, '-j', '96'],
                             cwd=self.kernelPath, env=env,
                             stdout=self.buildLog,
                             stderr=self.buildLog)
        p.wait()
        if p.returncode:
            raise Exception('build failed: %u' % p.returncode)

    def install(self):
        env = copy.copy(os.environ)
        env['LOCAL_VERSION'] = ''
        p = subprocess.Popen(['sudo', 'make', 'O=%s' % self.objPath, '-j', '48',
                              'install', 'modules_install'],
                             cwd=self.kernelPath, env=env,
                             stdout=subprocess.PIPE,
                             stderr=self.buildLog)
        version = None
        for line in p.stdout:
            self.buildLog.write(line)
            if line.find('DEPMOD') != -1:
                vals = line.split()
                version = vals[1]

        p.wait()
        if p.returncode:
            raise Exception('install failed: %u' % p.returncode)
        if not version:
            raise Exception('unable to find install version')
        return version

def reboot(name):
    p = subprocess.Popen(['sudo', 'greboot', name],
                         stdin=subprocess.PIPE)
    p.stdin.write('y\n')
    p.stdin.write('y\n')
    p.stdin.write('y\n')
    p.stdin.write('y\n')
    p.stdin.write('y\n')
    p.wait()
    if p.returncode:
        raise Exception('grebooot failed: %u' % p.returncode)
    print 'Rebooting...'
    exit(0)

def kexec(name):
    p = subprocess.Popen(['sudo', 'kexec', '-l', '/boot/vmlinuz-%s' % name,
                          '--append="%s"' % COMMAND_LINE])
    p.wait()
    if p.returncode:
        raise Exception('kexec -l failed: %u' % p.returncode)
    p = subprocess.Popen(['sync'])
    p.wait()
    p = subprocess.Popen(['sudo', 'kexec', '-e'])
    p.wait()
    raise Exception('kexec -e returned: %u' % p.returncode)

def reboot_default():
    p = subprocess.Popen(['sudo', 'shutdown', '-r', 'now'])
    p.wait()
    if p.returncode:
        raise Exception('shutdown failed: %u' % p.returncode)
    print 'Rebooting into default...'
    exit(0)

def main(argv=None):
    if argv is None:
        argv = sys.argv
    parse_args(argv)    

    name = os.uname()[2]
    if not DEBUG and not name.endswith('bisect'):
        print 'Not a bisect kernel'
        return

    if not DEBUG:
        fixup_motd()

    time.sleep(DELAY)

    gitLog = open('bisect-results/git-log', 'a')
    gitLog.write('----\n')
    buildLog = open('bisect-results/build-log', 'w+')
    bisector = BisectHelper('/home/sbw/linux-2.6', '/home/sbw/linux-2.6/obj', 
                            gitLog, buildLog)

    good = test_kernel()
    if DEBUG:
        exit(0)

    print 'bisecting ...\r\n',
    
    done = False
    if good:
        done = bisector.good()
    else:
        done = bisector.bad()

    if done:
        gitLog.close()    
        buildLog.close()
        reboot_default()

    print 'building ...\r\n',
    bisector.build()
    print 'installing ...\r\n',
    version = bisector.install()
    print 'rebooting %s ....\r\n' % version

    gitLog.close()    
    buildLog.close()
    if not DEBUG:
        reboot(version)

if __name__ == '__main__':
    sys.exit(main())
