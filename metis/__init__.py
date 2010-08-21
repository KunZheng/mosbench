from mparts.manager import Task
from mparts.host import HostInfo, CHECKED
from support import ResultsProvider, SetCPUs, FileSystem

import os, re

__all__ = []

__all__.append("Wrmem")
class Wrmem(Task, ResultsProvider):
    __config__ = ["host", "metisPath", "streamflow", "model", "trial"]

    def __init__(self, host, trial, metisPath, streamflow, model, cores):
        assert model in ["default", "hugetlb"], \
            "Unknown Metis memory model %r" % model

        Task.__init__(self, host = host, trial = trial)
        ResultsProvider.__init__(self, cores)
        self.host = host
        self.trial = trial
        self.metisPath = metisPath
        self.streamflow = streamflow
        self.model = model

    def wait(self, m):
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
        # XXX Use sysmon to get real time
        self.setResults(1, "job", "jobs", parseResults(log))

__all__.append("parseResults")
def parseResults(log):
    out = []
    realRe = re.compile(r".*\bReal:\s*([0-9]+)\s*$")
    for l in log.splitlines():
        m = realRe.match(l)
        if m:
            out.append(float(m.group(1))/1000)
    if not out:
        raise ValueError("Failed to parse results log")
    if len(out) > 1:
        raise ValueError("Multiple results found in results log")
    return out[0]

class Metis(object):
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
        if cfg.hotplug:
            m += SetCPUs(host = host, num = cfg.cores)
        for trial in range(cfg.trials):
            m += Wrmem(host, trial, metisPath, cfg.streamflow, cfg.model,
                       cfg.cores)
        # m += cfg.monitors
        m.run()

__all__.append("runner")
runner = Metis()
