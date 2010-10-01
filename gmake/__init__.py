from mparts.manager import Task
from mparts.host import HostInfo, CHECKED, UNCHECKED
from mparts.util import Progress
from support import ResultsProvider, SourceFileProvider, SetCPUs, PrefetchDir, \
    FileSystem, SystemMonitor

import os

__all__ = []

__all__.append("GmakeLoad")
class GmakeLoad(Task, ResultsProvider, SourceFileProvider):
    __info__ = ["host", "srcPath", "objPath", "*sysmonOut"]

    def __init__(self, host, trial, cores, srcPath, objPath, sysmon):
        Task.__init__(self, host = host, trial = trial)
        ResultsProvider.__init__(self, cores)
        self.host = host
        self.srcPath = srcPath
        self.objPath = objPath
        self.sysmon = sysmon
        # This configuration was built by 'make defconfig'
        self.__configFile = self.queueSrcFile(host, "x86_64_defconfig")

    def __cmd(self, target):
        return ["make", "-C", self.srcPath, "O=" + self.objPath,
                "-j", str(self.cores*2), target]

    def wait(self, m):
        logPath = self.host.getLogPath(self)

        # Copy configuration file
        self.host.r.run(["cp", self.__configFile,
                         os.path.join(self.objPath, ".config")])

        with Progress("Preparing build"):
            # Clean
            self.host.r.run(self.__cmd("clean"), stdout = logPath)

            # Build init/main.o first.  This gets most of the serial
            # early build stages out of the way.
            self.host.r.run(self.__cmd("init/main.o"), stdout = logPath)

        # Build for real
        #
        # XXX If we want to eliminate the serial startup, monitor
        # starting with "  CHK include/generated/compile.h" or maybe
        # with the first "  CC" line.
        self.host.r.run(self.sysmon.wrap(self.__cmd("vmlinux.o")),
                        stdout = logPath)

        # Get result
        log = self.host.r.readFile(logPath)
        self.sysmonOut = self.sysmon.parseLog(log)
        self.setResults(1, "build", "builds", self.sysmonOut["time.real"])

class GmakeRunner(object):
    def __str__(self):
        return "gmake"

    @staticmethod
    def run(m, cfg):
        host = cfg.primaryHost
        m += host
        m += HostInfo(host)
        fs = FileSystem(host, cfg.fs, clean = True)
        m += fs
        # It's really hard to predict what make will access, so we
        # prefetch the whole source tree.  This, combined with the
        # pre-build of init/main.o, eliminates virtually all disk
        # reads.  For the rest, we'll just have to rely on multiple
        # trials or at least multiple configurations to cache.
        m += PrefetchDir(host, cfg.kernelRoot, ["*/.git"])
        if cfg.hotplug:
            m += SetCPUs(host = host, num = cfg.cores)
        sysmon = SystemMonitor(host)
        m += sysmon
        for trial in range(cfg.trials):
            m += GmakeLoad(host, trial, cfg.cores, cfg.kernelRoot, fs.path + "0",
                           sysmon)
        # m += cfg.monitors
        m.run()

__all__.append("runner")
runner = GmakeRunner()
