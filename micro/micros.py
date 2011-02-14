import subprocess
import re

def get_cpu_ghz():
    f = open('/proc/cpuinfo', 'r')
    for l in f:
        m = re.search('cpu MHz\s+: (\d+\.\d+)', l)
        if m:
            return float(m.group(1)) / 1000.0
    return None

class FopsDir(object):
    baseFileName = '/root/tmp/foo'
    
    def __init__(self):
        pass

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
        if self.schedOp == 'sleep':
            nprocs = ncores * 20
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
