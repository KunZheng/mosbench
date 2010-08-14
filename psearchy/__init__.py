from mparts.manager import Task
from mparts.host import HostInfo, CHECKED, UNCHECKED
from mparts.util import Progress
from support import BenchmarkRunner, SetCPUs, PrefetchList, FileSystem

import os, re

__all__ = []

__all__.append("Mkdb")
class Mkdb(Task, BenchmarkRunner):
    MODE_THREAD = intern("thread")
    MODE_PROCESS = intern("process")

    ORDER_SEQ = intern("seq")
    ORDER_RR = intern("rr")

    __config__ = ["host", "psearchyPath", "filesPath", "dbPath",
                  "mode", "order", "mem"]

    def __init__(self, host, psearchyPath, filesPath, dbPath, cores,
                 mode = MODE_THREAD, order = ORDER_SEQ,
                 mem = 256, trials = 3):
        assert mode in [Mkdb.MODE_THREAD, Mkdb.MODE_PROCESS], \
            "Invalid mode %s" % mode
        assert order in [Mkdb.ORDER_SEQ, Mkdb.ORDER_RR], \
            "Invalid order %s" % order

        Task.__init__(self, host = host)
        BenchmarkRunner.__init__(self, cores, trials)
        self.host = host
        self.psearchyPath = psearchyPath
        self.filesPath = filesPath
        self.dbPath = dbPath
        self.mode = mode
        self.order = order
        self.mem = mem

    def runTrial(self, m, trial):
        # Construct command
        cmd = [os.path.join(self.psearchyPath, "mkdb", "pedsort"),
               "-t", self.dbPath,
               "-c", str(self.cores),
               "-m", str(self.mem)]
        if self.mode == Mkdb.MODE_PROCESS:
            cmd.append("-p")
        if self.order == Mkdb.ORDER_RR:
            cmd.extend(["-s", "1"])

        # Run
        logPath = self.host.getLogPath(self)
        self.host.r.run(cmd, stdin = self.filesPath, stdout = logPath,
                        wait = CHECKED)

        # Get result
        log = self.host.r.readFile(logPath)
        return parseResults(log)[trial]

__all__.append("parseResults")
def parseResults(log):
    out = []
    tputRe = re.compile(r"[0-9]+:.*\bthroughput:\s*([0-9]+\.?[0-9]*)\s+(\S+)\s*$")
    for line in log.splitlines():
        m = tputRe.match(line)
        if m:
            out.append((float(m.group(1)), m.group(2)))
    if not out:
        raise ValueError("Failed to parse results log")
    return out

__all__.append("Mkfiles")
class Mkfiles(Task):
    __config__ = ["host", "psearchyPath", "baseDir"]

    def __init__(self, host, psearchyPath, baseDir):
        Task.__init__(self, host = host)
        self.host = host
        self.psearchyPath = psearchyPath
        self.baseDir = baseDir

        self.filesPath = \
            os.path.join(self.psearchyPath, "files-%08x" % abs(hash(baseDir)))

    def start(self):
        # Do I need to construct the files list?
        test = self.host.r.run(["test", "-f", self.filesPath],
                               wait = UNCHECKED)
        if test.getCode() != 0:
            # Yes
            with Progress("Generating files list"):
                self.host.r.run([os.path.join(self.psearchyPath, "mkfiles"),
                                 self.baseDir],
                                stdout = self.filesPath + ".tmp",
                                noCheck = True)
                self.host.r.run(["mv", self.filesPath + ".tmp",
                                 self.filesPath])

class Psearchy(object):
    def __str__(self):
        return "psearchy"

    @staticmethod
    def run(m, cfg):
        host = cfg.primaryHost
        m += host
        m += HostInfo(host)
        fs = FileSystem(host, cfg.fs, clean = True)
        m += fs
        psearchyPath = os.path.join(cfg.benchRoot, "psearchy")
        files = Mkfiles(host, psearchyPath, cfg.textRoot)
        m += files
        m += PrefetchList(host, files.filesPath, reuse = True)
        if cfg.hotplug:
            m += SetCPUs(host = host, num = cfg.cores, seq = cfg.order)
        m += Mkdb(host, psearchyPath, files.filesPath, fs.path,
                  cfg.cores, cfg.mode, cfg.order, cfg.mem, cfg.trials)
        # m += cfg.monitors
        m.run()

__all__.append("runner")
runner = Psearchy()
