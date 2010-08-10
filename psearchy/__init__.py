from mparts.manager import Task
from mparts.host import CHECKED, UNCHECKED
from mparts.util import Progress
from support import SetCPUs, PrefetchList

import os

__all__ = []

__all__.append("Mkdb")
class Mkdb(Task):
    MODE_THREAD = intern("thread")
    MODE_PROCESS = intern("process")

    ORDER_SEQ = intern("seq")
    ORDER_RR = intern("rr")

    __config__ = ["host", "binPath", "filesPath", "dbPath",
                  "ncores", "mode", "order", "mem"]

    def __init__(self, host, binPath, filesPath, dbPath, ncores,
                 mode = MODE_THREAD, order = ORDER_SEQ,
                 mem = 256):
        assert mode in [Mkdb.MODE_THREAD, Mkdb.MODE_PROCESS], \
            "Invalid mode %s" % mode
        assert order in [Mkdb.ORDER_SEQ, Mkdb.ORDER_RR], \
            "Invalid order %s" % order

        Task.__init__(self, host = host)
        self.host = host
        self.binPath = binPath
        self.filesPath = filesPath
        self.dbPath = dbPath
        self.ncores = ncores
        self.mode = mode
        self.order = order
        self.mem = mem

    def wait(self):
        cmd = [os.path.join(self.binPath, "mkdb", "pedsort"),
               "-t", self.dbPath,
               "-c", str(self.ncores),
               "-m", str(self.mem)]
        if self.mode == Mkdb.MODE_PROCESS:
            cmd.append("-p")
        if self.order == Mkdb.ORDER_RR:
            cmd.extend(["-s", "1"])

        logPath = self.host.getLogPath(self)
        self.host.r.run(cmd, stdin = self.filesPath, stdout = logPath,
                        wait = CHECKED)

        # XXX Get result

__all__.append("Mkfiles")
class Mkfiles(Task):
    __config__ = ["host", "binPath", "baseDir"]

    def __init__(self, host, binPath, baseDir):
        Task.__init__(self, host = host)
        self.host = host
        self.binPath = binPath
        self.baseDir = baseDir

        self.filesPath = \
            os.path.join(self.binPath, "files-%08x" % abs(hash(baseDir)))

    def start(self):
        # Do I need to construct the files list?
        test = self.host.r.run(["test", "-f", self.filesPath],
                               wait = UNCHECKED)
        if test.getCode() != 0:
            # Yes
            with Progress("Generating files list"):
                self.host.r.run([os.path.join(self.binPath, "mkfiles"),
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
        # XXX Clean up output directories between runs?
        host = cfg.primaryHost
        m += host
        binPath = os.path.join(cfg.benchRoot, "psearchy")
        files = Mkfiles(host, binPath, cfg.textRoot)
        m += files
        m += PrefetchList(host, files.filesPath, reuse = True)
        if cfg.hotplug:
            m += SetCPUs(host = host, num = cfg.cores, seq = cfg.order)
        m += Mkdb(host, binPath, files.filesPath, "/tmp/mosbench/%s/" % cfg.fs,
                  cfg.cores, cfg.mode, cfg.order, cfg.mem)
    #    m += cfg.monitors
        m.run()

__all__.append("runner")
runner = Psearchy()