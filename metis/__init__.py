from mparts.manager import Task
from mparts.host import HostInfo, CHECKED
from support import ResultsProvider, SetCPUs, FileSystem, SystemMonitor

import os

__all__ = []

__all__.append("MetisLoad")
class MetisLoad(Task, ResultsProvider):
    __config__ = ["host", "trial", "metisPath", "streamflow", "model",
                  "*sysmonOut"]

    def __init__(self, host, trial, cores, metisPath, streamflow, model,
                 setcpus, sysmon):
        assert model in ["default", "hugetlb"], \
            "Unknown Metis memory model %r" % model

        Task.__init__(self, host = host, trial = trial)
        ResultsProvider.__init__(self, cores)
        self.host = host
        self.trial = trial
        self.metisPath = metisPath
        self.streamflow = streamflow
        self.model = model
        self.setcpus = setcpus
        self.sysmon = sysmon

    def wait(self, m):
        cpuseq = ",".join(map(str, self.setcpus.getSeq()))

        obj = os.path.join(self.metisPath, "obj." + self.model)
        cmd = [os.path.join(obj, "app",
                            "wrmem" + (".sf" if self.streamflow else "")),
               "-p", str(self.cores)]
        cmd = self.sysmon.wrap(cmd, "Starting mapreduce", "Finished mapreduce")
        addEnv = {"LD_LIBRARY_PATH" : os.path.join(obj, "lib"),
                  "CPUSEQ" : cpuseq}

        # Run
        logPath = self.host.getLogPath(self)
        self.host.r.run(cmd, stdout = logPath, addEnv = addEnv, wait = CHECKED)

        # Get result
        log = self.host.r.readFile(logPath)
        self.sysmonOut = self.sysmon.parseLog(log)
        self.setResults(1, "job", "jobs", self.sysmonOut["time.real"])

class MetisRunner(object):
    def __str__(self):
        return "metis"

    @staticmethod
    def run(m, cfg):
        # XXX Clean hugetlb directories between trials?
        host = cfg.primaryHost
        m += host
        m += HostInfo(host)
        if cfg.model == "hugetlb":
            fs = FileSystem(host, "hugetlb", clean = True)
            m += fs
        metisPath = os.path.join(cfg.benchRoot, "metis")
        setcpus = SetCPUs(host = host, num = cfg.cores, hotplug = cfg.hotplug,
                          seq = cfg.order)
        m += setcpus
        sysmon = SystemMonitor(host)
        m += sysmon
        for trial in range(cfg.trials):
            m += MetisLoad(host, trial, cfg.cores, metisPath, cfg.streamflow,
                           cfg.model, setcpus, sysmon)
        # m += cfg.monitors
        m.run()

__all__.append("runner")
runner = MetisRunner()
