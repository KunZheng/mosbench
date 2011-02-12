import subprocess
import re

def get_cpu_ghz():
    f = open('/proc/cpuinfo', 'r')
    for l in f:
        m = re.search('cpu MHz\s+: (\d+\.\d+)', l)
        if m:
            return float(m.group(1)) / 1000.0
    return None

class FopsDir:
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

class Memclone:
    useThreads = 0
    mbytes = 200

    def __init__(self):
        pass

    def run(self, ncores, duration):
        ghz = get_cpu_ghz()
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

class Populate:
    useThreads = 0
    mbytes = 200

    def __init__(self):
        pass

    def run(self, ncores, duration):
        ghz = get_cpu_ghz()
        p = subprocess.Popen(["o/populate", str(duration), str(ncores), 
                              str(self.mbytes), str(self.useThreads)],
                             stdout=subprocess.PIPE)
        p.wait()
        if p.returncode:
            raise Exception('Populate.run failed: %u' % p.returncode)
        l = p.stdout.readline().strip()
        m = re.search('rate: (\d+\.\d+) per sec', l)
        return float(m.group(1))

    def get_name(self):
        if self.useThreads:
            return 'populate-threads'
        else:
            return 'populate-processes'
