from mparts.manager import Task
from mparts.host import HostInfo, CHECKED
from support import SetCPUs, FileSystem

import os

__all__ = []

__all__.append("Wrmem")
class Wrmem(Task):
    __config__ = ["host", "metisPath", "streamflow", "model", "cores"]

    def __init__(self, host, metisPath, streamflow, model, cores):
        assert model in ["default", "hugetlb"], \
            "Unknown Metis memory model %r" % model

        Task.__init__(self, host = host)
        self.host = host
        self.metisPath = metisPath
        self.streamflow = streamflow
        self.model = model
        self.cores = cores

    def wait(self):
        obj = os.path.join(self.metisPath, "obj." + self.model)
        cmd = [os.path.join(obj, "app",
                            "wrmem" + (".sf" if self.streamflow else "")),
               "-p", str(self.cores)]
        addEnv = {"LD_LIBRARY_PATH" : os.path.join(obj, "lib")}

        # Run
        logPath = self.host.getLogPath(self)
        self.host.r.run(cmd, stdout = logPath, addEnv = addEnv, wait = CHECKED)

        # XXX Get result

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
        m += Wrmem(host, metisPath, cfg.streamflow, cfg.model, cfg.cores)
        # m += cfg.monitors
        m.run()

__all__.append("runner")
runner = Metis()
