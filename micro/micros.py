import subprocess
import time
import re
import os.path
import os

def get_cpu_ghz():
    f = open('/proc/cpuinfo', 'r')
    for l in f:
        m = re.search('cpu MHz\s+: (\d+\.\d+)', l)
        if m:
            return float(m.group(1)) / 1000.0
    return None

class FopsDir(object):
    def __init__(self, baseFileName='/root/tmp/foo'):
        self.baseFileName = baseFileName

    def run(self, ncores, duration):
        p = subprocess.Popen(["o/fops-dir", str(duration), str(ncores), 
                              self.baseFileName, '0'],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('FopsDir.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        m = re.search('rate: (\d+\.\d+) per sec', l)
        return float(m.group(1))

    def get_name(self):
        return 'fops-dir'

class Memclone(object):
    useThreads = 0
    mbytes = 200

    def __init__(self):
        pass

    def run(self, ncores, duration):
        p = subprocess.Popen(["o/memclone", str(ncores), str(self.mbytes),
                              str(self.useThreads)],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('Memclone.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        m = re.search('rate: (\d+\.\d+) per sec', l)
        return float(m.group(1))

    def get_name(self):
        if self.useThreads:
            return 'memclone-threads'
        else:
            return 'memclone-processes'

class Memmap(object):
    useThreads = 0

    def __init__(self, kbytes = 64):
        self.kbytes = kbytes

    def run(self, ncores, duration):
        p = subprocess.Popen(["o/memmap", str(duration), str(ncores), 
                              str(self.kbytes), str(self.useThreads), str(0)],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('Memmap.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        m = re.search('rate: (\d+\.\d+) per sec', l)
        return float(m.group(1))

    def get_name(self):
        if self.useThreads:
            return 'memmap-threads-%u' % self.kbytes
        else:
            return 'memmap-processes-%u' % self.kbytes

class Mempop(object):
    useThreads = 0

    def __init__(self, kbytes = 16):
        self.kbytes = kbytes

    def run(self, ncores, duration):
        p = subprocess.Popen(["o/memmap", str(duration), str(ncores), 
                              str(self.kbytes), str(self.useThreads), str(1)],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('Mempop.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        m = re.search('rate: (\d+\.\d+) per sec', l)
        return float(m.group(1))

    def get_name(self):
        if self.useThreads:
            return 'mempop-threads-%u' % self.kbytes
        else:
            return 'mempop-processes-%u' % self.kbytes

class Procy(object):
    useThreads = 0
    
    def __init__(self, schedOp = 'create-proc'):
        self.schedOp = schedOp

    def run(self, ncores, duration):
        nprocs = ncores
        if self.schedOp == 'yield':
            nprocs = ncores * 16
        p = subprocess.Popen(["o/procy", 
                              '-time', str(duration), 
                              '-ncores', str(ncores), 
                              '-nprocs', str(nprocs),
                              '-use_threads', str(self.useThreads),
                              '-sched_op', self.schedOp],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('Procy.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        m = re.search('rate: (\d+\.\d+) per sec', l)
        return float(m.group(1))

    def get_name(self):
        if self.useThreads:
            return 'procy-threads-%s' % self.schedOp
        else:
            return 'procy-processes-%s' % self.schedOp

class ProcyExec(object):
    execServer='./exec-server.py'

    def __init__(self, execOp='create-proc'):
        self.execOp = execOp

    def run(self, ncores, duration):
        p = subprocess.Popen([self.execServer, str(duration), str(ncores), 
                              self.execOp],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('ProcyExec.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        return float(l)

    def get_name(self):
        return 'procy-exce-%s' % self.execOp

class BFish(object):
    def __init__(self, nclines=1, nsets=128, bfishCommand='o/bfish-shared'):
        self.nclines = nclines
        self.nsets = nsets
        self.bfishCommand = bfishCommand

    def run(self, ncores, duration):
        args = []
        if self.bfishCommand.find('msg') != -1:
            args = [self.bfishCommand, str(ncores), str(ncores), 
                    str(self.nclines), 
                    str(self.nsets), str(duration)]
        else:
            args = [self.bfishCommand, str(ncores), str(self.nclines), 
                    str(self.nsets), str(duration)]

        p = subprocess.Popen(args, stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('BFishShared.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        return float(l.split()[1])

    def get_name(self):
        return os.path.basename(self.bfishCommand) + '-' + str(self.nclines)

class Exim(object):
    mosbenchPath='/home/sbw/mosbench'
    
    def __init__(self, logPath=None):
        if not logPath:
            logPath = '/tmp/micros-Exim-%u-log' % os.getpid()
        self.logFile = open(logPath, 'w')

    def run(self, ncores, duration):
        # XXX the path could exist, and not be mounted
        if not os.path.exists('/tmp/mosbench/tmpfs-separate'):
            p = subprocess.Popen(['sudo', './mkmounts', 'tmpfs-separate'],
                                 cwd=self.mosbenchPath)
            p.wait()
            if p.returncode:
                raise Exception('mkmounts failed: %u' % p.returncode)            

        p = subprocess.Popen(['sed', '-i', '-e', 
                              's/NUM_CORES=[1-9][0-9]*/NUM_CORES=%u/g' % ncores,
                              self.mosbenchPath + '/config.py'])
        p.wait()
        if p.returncode:
            raise Exception('Exim.run sed failed: %u' % p.returncode)
        
        p = subprocess.Popen(['make', 'bench'],
                             cwd=self.mosbenchPath, 
                             stdout=subprocess.PIPE,
                             stderr=self.logFile)

        tp = 0.0
        while True:
            if p.stdout.closed:
                break
            if p.returncode:
                break
            l = p.stdout.readline()
            if l == '':
                break
            m = re.search('.*\=\> (\d+) messages \((\d+\.\d+) secs.*', l)
            if m:
                tp = float(m.group(1)) / float(m.group(2))

        p.wait()
        if p.returncode:
            raise Exception('Exim.run make bench failed: %u' % p.returncode)
        time.sleep(1)
        return tp

    def get_name(self):
        return 'exim'
