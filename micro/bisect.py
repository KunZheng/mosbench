#!/usr/bin/python
import subprocess
import datetime
import results
import pickle
import micros
import errno
import copy
import time
import sys
import re
import os

#
# To start of the bisect process:
#   $ ./bisect.py -bad bad-kernel -good good-kernel
# or by hand:
#   $ cd linux
#   $ git bisect start BAD GOOD
#   $ make O=obj -j96
#   # make O=obj install modules_install -j48
#   # greboot ....
#

# Config
BENCHMARK     = micros.Exim(logPath=('historical-log/%u-log' % os.getpid()))
MIN_TPUT      = 11500.0
ENABLE        = True

# Other knobs
RESULT_BASE   = 'bisect-results'
START_CORE    = 40
STOP_CORE     = 48
DURATION      = 15
NUM_RUNS      = 1
DELAY         = 0
DEBUG         = False
BAD_REF       = None
GOOD_REF      = None

def usage(argv):
    print '''Usage: %s benchmark-name [ -start start -stop stop -duration duration
  -good good -bad bad ]
    'start' is starting core count
    'stop' is ending core count
    'duration' is the duration of each run
    'delay' is the number of seconds to delay before starting
    'debug' is True to enable debugging
    'good' is good kernel ref. spec.
    'bad' is bad kernel ref.spec.
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

    def good_handler(good):
        global GOOD_REF
        GOOD_REF = good
        
    def bad_handler(bad):
        global BAD_REF
        BAD_REF = bad

    handler = {
        '-start'     : start_handler,
        '-stop'      : stop_handler,
        '-duration'  : duration_handler,
        '-delay'     : delay_handler,
        '-debug'     : debug_handler,
        '-good'      : good_handler,
        '-bad'       : bad_handler
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

See '# screen -x tom' on hooverdam for more information.

'''
    p = subprocess.Popen(['sudo', 'sh', '-c', 'echo -ne "%s" > /etc/motd' % msg])
    p.wait()

def bad_kernel_exit():
    exit(1)

def good_kernel_exit():
    exit(0)

class MinResult(object):
    def __init__(self, minValue):
        self.minValue = minValue
        self.bad = False
        self.good = True

    def new_value(self, value):
        if value >= self.minValue:
            self.bad = True

    def done(self):
        return self.bad

def test_kernel():

    maxTp = (0, 0)
    for c in range(START_CORE, STOP_CORE + 1):
        tp = 0
        for x in range(0, NUM_RUNS):
            t = BENCHMARK.run(c, DURATION)
            if t > tp:
                tp = t

        if tp > maxTp[1]:
            maxTp = (c, tp)
        print 'bisect: %s %u / %u -- %f (max tp %u, %f)\r\n' % (BENCHMARK.get_name(), 
                                                                c, STOP_CORE, tp,
                                                                maxTp[0], maxTp[1]),
        if maxTp[1] > MIN_TPUT:
            print 'good kernel (min tp %f)\r\n' % MIN_TPUT,
            return True
    
    print 'bad kernel (min tp %f)\r\n' % MIN_TPUT,
    return False

class BisectHelper(object):
    class BuildException(Exception):
        def __init__(self, value):
            self.value = value
        def __str__(self):
            return repr(self.value) 

    def __init__(self, kernelPath, objPath, gitLog, buildLog):
        self.kernelPath = kernelPath
        self.objPath = objPath
        self.gitLog = gitLog
        self.buildLog = buildLog

    def __git_bisect(self, command, extraArgs = []):
        args = ['git', 'bisect', command]
        args.extend(extraArgs)
        p = subprocess.Popen(args,
                             cwd=self.kernelPath,
                             stdout=subprocess.PIPE)
        done = False
        for l in p.stdout:
            self.gitLog.write(l)
            self.gitLog.flush()
            if l.find('is the first bad commit') != -1:
                done = True
        p.wait()
        if p.returncode:
            raise Exception('git bisect %s failed: %u' % (command, p.returncode))
        return done
        
    def good(self):
        return self.__git_bisect('good')

    def bad(self):
        return self.__git_bisect('bad')

    def skip(self):
        return self.__git_bisect('skip')        

    def start(self, badRef, goodRef):
        self.__git_bisect('start', extraArgs = [badRef, goodRef])

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
            raise BisectHelper.BuildException('build failed: %u' % p.returncode)

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

def reboot_default():
    p = subprocess.Popen(['sudo', 'shutdown', '-r', 'now'])
    p.wait()
    if p.returncode:
        raise Exception('shutdown failed: %u' % p.returncode)
    print 'Rebooting into default...'
    exit(0)

def main(argv=None):
    if not ENABLE:
        exit(0)

    if argv is None:
        argv = sys.argv
    parse_args(argv)    

    startMode = False
    if GOOD_REF and BAD_REF:
        startMode = True

    name = os.uname()[2]
    if not startMode and not DEBUG and not name.endswith('bisect'):
        print 'Not a bisect kernel'
        return

    if not DEBUG:
        fixup_motd()

    if not startMode:
        time.sleep(DELAY)

    gitLog = None
    buildLog = None
    resultPath = None

    if startMode:
        now = datetime.datetime.now()
        resultPath = RESULT_BASE + '/' + BENCHMARK.get_name()
        resultPath += '.' + str(now.month)
        resultPath += '.' + str(now.day)
        resultPath += '.' + str(now.year)
        resultPath += '.' + str(os.getpid())
        os.mkdir(resultPath)
        status = open(RESULT_BASE + '/' + 'status', 'w')
        status.write(resultPath)
        status.close()
    else:
        status = open(RESULT_BASE + '/' + 'status', 'r')
        resultPath = status.readline()
        status.close()

    gitLog = open(resultPath + '/git-log', 'a')
    gitLog.write('----\n')
    buildLog = open(resultPath + '/build-log', 'w+')
    bisector = BisectHelper('/home/sbw/linux-2.6', '/home/sbw/linux-2.6/obj', 
                            gitLog, buildLog)

    if not startMode:
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
    else:
        print 'starting (%s)...\r\n' % resultPath,
        bisector.start(BAD_REF, GOOD_REF)


    print 'building ...\r\n',
    buildTries = 0
    while True:
        try:
            buildTries += 1
            bisector.build()
            break;
        except BisectHelper.BuildException as e:
            print 'oops, build failed: %s\r\n' % e.value,
            if buildTries > 3:
                print 'too many broken builds, giving up...\r\n'
                gitLog.write('* Too many broken builds *\n')
                gitLog.close()
                buildLog.close()
                reboot_default()
            print 'bisect skip and try again\r\n'
            bisector.skip()

    print 'installing ...\r\n',
    version = bisector.install()
    print 'rebooting %s ....\r\n' % version

    gitLog.close()    
    buildLog.close()
    if not DEBUG:
        reboot(version)

if __name__ == '__main__':
    sys.exit(main())
