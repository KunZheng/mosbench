from mparts.manager import Task
from mparts.host import HostInfo, CHECKED
from support import BenchmarkRunner, SetCPUs, FileSystem

import os, re

__all__ = []

__all__.append("Wrmem")
class Wrmem(Task, BenchmarkRunner):
    __config__ = ["host", "metisPath", "streamflow", "model"]

    def __init__(self, host, metisPath, streamflow, model, cores, trials):
        assert model in ["default", "hugetlb"], \
            "Unknown Metis memory model %r" % model

        Task.__init__(self, host = host)
        BenchmarkRunner.__init__(self, cores, trials)
        self.host = host
        self.metisPath = metisPath
        self.streamflow = streamflow
        self.model = model

    def runTrial(self, m):
        obj = os.path.join(self.metisPath, "obj." + self.model)
        cmd = [os.path.join(obj, "app",
                            "wrmem" + (".sf" if self.streamflow else "")),
               "-p", str(self.cores)]
        addEnv = {"LD_LIBRARY_PATH" : os.path.join(obj, "lib")}

        # Run
        logPath = self.host.getLogPath(self)
        self.host.r.run(cmd, stdout = logPath, addEnv = addEnv, wait = CHECKED)

        # Get result
        log = self.host.r.readFile(logPath)
        return parseResults(log)[-1]

__all__.append("parseResults")
def parseResults(log):
    out = []
    coreRe = re.compile(r"Runtime in milliseconds \[([0-9]+) cores\]")
    realRe = re.compile(r".*\bReal:\s*([0-9]+)\s*$")
    for l in log.splitlines():
        m = coreRe.match(l)
        if m:
            cores = int(m.group(1))
        else:
            m = realRe.match(l)
            if m:
                real = float(m.group(1))
                # Compute jobs/hour/core from ms/job
                out.append(((60*60*1000) / (real*cores), "jobs/hour/core"))
    if not out:
        raise ValueError("Failed to parse results log")
    return out

class Metis(object):
    def __str__(self):
        return "metis"

    @staticmethod
    def run(m, cfg):
        # XXX Clean hugetlb directories
        host = cfg.primaryHost
        m += host
        m += HostInfo(host)
        if cfg.model == "hugetlb":
            fs = FileSystem(host, "hugetlb", clean = True)
            m += fs
        metisPath = os.path.join(cfg.benchRoot, "metis")
        if cfg.hotplug:
            m += SetCPUs(host = host, num = cfg.cores)
        m += Wrmem(host, metisPath, cfg.streamflow, cfg.model, cfg.cores,
                   cfg.trials)
        # m += cfg.monitors
        m.run()

__all__.append("runner")
runner = Metis()
