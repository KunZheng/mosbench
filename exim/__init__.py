from mparts.manager import Task
from mparts.host import HostInfo
from support import ResultsProvider, SetCPUs, FileSystem, SystemMonitor, waitForLog

import os, signal, re

__all__ = []

CORES = file("/proc/cpuinfo").read().count("processor\t")

class EximLauncher:
    def __init__(self, host):
        self.host = host

    def run(self, cmdArgs):
        return self.host.r.run(cmdArgs, wait = False)

    def runOnCpu(self, cpu, cmdArgs):
        cmd = [ "numactl", "-C", str(cpu) ]
        cmd.extend(cmdArgs)
        return self.host.r.run(cmd, wait = False)

    def importFile(self, path):
        pass

class EximChrooter:
    def __init__(self, path, host):
        self.path = path
        self.host = host

    def run(self, cmdArgs):
        cmd = [ "chroot", self.path ]
        cmd.extend(cmdArgs)
        return self.host.r.run(cmd, wait = False)

    def runOnCpu(self, cpu, cmdArgs):
        cmd = [ "numactl", "-C", str(cpu), "chroot", self.path ]
        cmd.extend(cmdArgs)
        return self.host.r.run(cmd, wait = False)

    def importFile(self, path):
        self.host.r.run(["rsync", "-aR", path, self.path])

__all__.append("EximDaemon")
class EximDaemon(Task):
    __info__ = ["host", "eximPath", "eximBuild", "mailDir", "spoolDir", "port", 
                "numInstances", "chrootBase"]

    def __init__(self, host, eximPath, eximBuild, mailDir, spoolDir, port, 
                 numInstances, chrootBase):
        Task.__init__(self, host = host)
        self.host = host
        self.eximPath = eximPath
        self.eximBuild = eximBuild
        self.mailDir = mailDir
        self.spoolDir = spoolDir
        self.port = port
        self.numInstances = numInstances
        self.__proc = []
        self.chrootBase = chrootBase

    def __iPath(self, string, i):
        return string + "-" + str(i)

    def __start(self, i, chrootBase = None):
        launcher = None
        if chrootBase == None:
            launcher = EximLauncher(self.host)
        else:
            launcher = EximChrooter(chrootBase, self.host)

        # Create configuration
        config = self.host.outDir(self.__iPath(self.name + ".configure", i))
        self.host.r.run(
            [os.path.join(self.eximPath, "mkconfig"),
             os.path.join(self.eximPath, self.__iPath(self.eximBuild, i)),
             self.mailDir + str(i), self.__iPath(self.spoolDir, i)],
            stdout = config)

        self.host.r.run(
            ["rm", "-f", os.path.join(self.__iPath(self.spoolDir, i), "log", "mainlog")])

        launcher.importFile(config)

        # Start Exim
        if self.numInstances > 1:
            proc  = launcher.runOnCpu(
                i % CORES, 
                [os.path.join(self.eximPath, self.__iPath(self.eximBuild, i), "bin", "exim"),
                 "-bdf", "-oX", str(self.port + i), "-C", config])
            self.__proc.append(proc)
        else:
            proc  = launcher.run(
                [os.path.join(self.eximPath, self.__iPath(self.eximBuild, i), "bin", "exim"),
                 "-bdf", "-oX", str(self.port + i), "-C", config])
            self.__proc.append(proc)

        if chrootBase == None:
            waitForLog(self.host, 
                       os.path.join(self.__iPath(self.spoolDir, i), "log", "mainlog"),
                       "exim", 5, "listening for SMTP")
        else:
            waitForLog(self.host, 
                       chrootBase + os.path.join(self.__iPath(self.spoolDir, i), "log", "mainlog"),
                       "exim", 5, "listening for SMTP")

    def start(self):
        for i in range(0, self.numInstances):
            if self.chrootBase:
                self.__start(i, chrootBase = self.chrootBase + "-" + str(i))
            else:
                self.__start(i)

    def stop(self):
        for p in self.__proc:
            # Ugh, there's no way to cleanly shut down Exim, so we can't
            # check for a sensible exit code.
            p.kill(signal.SIGTERM)
        self.__proc = []

    def reset(self):
        if len(self.__proc) > 0:
            self.stop()

__all__.append("EximLoad")
class EximLoad(Task, ResultsProvider):
    __info__ = ["host", "trial", "eximPath", "clients", "port", "*sysmonOut", "numInstances"]

    # XXX Control warmup/duration
    def __init__(self, host, trial, eximPath, cores, clients, port, sysmon, numInstances):
        Task.__init__(self, host = host, trial = trial)
        ResultsProvider.__init__(self, cores)
        self.host = host
        self.trial = trial
        self.eximPath = eximPath
        self.clients = clients
        self.port = port
        self.sysmon = sysmon
        self.numInstances = numInstances

    def wait(self):
        # We may want to wipe out old mail files, but it doesn't seem
        # to make a difference.

        cmd = [os.path.join(self.eximPath, "run-smtpbm"),
               str(self.clients), str(self.port), str(self.numInstances)]
        cmd = self.sysmon.wrap(cmd, "Starting", "Stopped")

        # Run
        logPath = self.host.getLogPath(self)
        self.host.r.run(cmd, stdout = logPath)

        # XXX Sanity check no paniclog or rejectlog, non-empty mboxes,
        # non-empty mainlog

        # Get result
        log = self.host.r.readFile(logPath)
        self.sysmonOut = self.sysmon.parseLog(log)
        ms = re.findall("(?m)^([0-9]+) messages", log)
        if len(ms) != 1:
            raise RuntimeError("Expected 1 message count in log, got %d",
                               len(ms))
        self.setResults(int(ms[0]), "message", "messages",
                        self.sysmonOut["time.real"])

class EximRunner(object):
    def __str__(self):
        return "exim"

    @staticmethod
    def run(m, cfg):
        if not cfg.hotplug:
            raise RuntimeError("The Exim benchmark requires hotplug = True.  "
                               "Either enable hotplug or disable the Exim "
                               "benchmark in config.py.")

        host = cfg.primaryHost
        m += host
        m += HostInfo(host)
        fs = FileSystem(host, cfg.fs, clean = True)
        m += fs
        eximPath = os.path.join(cfg.benchRoot, "exim")
        m += SetCPUs(host = host, num = cfg.cores)

        m += EximDaemon(host, eximPath, cfg.eximBuild,
                        os.path.join(fs.path),
                        os.path.join(fs.path + "spool"),
                        cfg.eximPort,
                        cfg.numInstances,
                        cfg.eximChrootBase)
        sysmon = SystemMonitor(host)
        m += sysmon
        for trial in range(cfg.trials):
            # XXX It would be a pain to make clients dependent on
            # cfg.cores.
            m += EximLoad(host, trial, eximPath, cfg.cores,
                          cfg.clients, cfg.eximPort, sysmon, cfg.numInstances)
        # m += cfg.monitors
        m.run()

__all__.append("runner")
runner = EximRunner()
