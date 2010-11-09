from mparts.manager import Task
from mparts.host import HostInfo, SourceFileProvider, STDERR
from mparts.util import Async
from support import ResultsProvider, SetCPUs, ExplicitSystemMonitor, IXGBE

import os, time, re

__all__ = []

BASE_PORT = 11211
STARTTIME = 2
SYNCTIME = 5
DURATION = 15

__all__.append("Memcached")
class Memcached(Task):
    __info__ = ["host", "mcPath", "mcBuild", "ports", "threadsPerCore", "mem"]

    def __init__(self, host, mcPath, mcBuild, ports, threadsPerCore, mem):
        Task.__init__(self, host = host)
        self.setInfoAttrs(Memcached, locals())
        self.__ps = []

    def start(self):
        bin = os.path.join(self.mcPath, self.mcBuild, "bin", "memcached")
        for core, port in enumerate(self.ports):
            p = self.host.r.run(
                ["numactl", "-C", str(core), "--", bin, "-m", str(self.mem),
                 "-t", str(self.threadsPerCore), "-p", str(port),
                 "-U", str(port)],
                wait = False)
            self.__ps.append(p)

        # Sleep for a second and make sure they're all still running
        time.sleep(1)
        for p in self.__ps:
            if p.wait(poll = True) != None:
                raise RuntimeError("memcached process exited early")

    def stop(self):
        self.reset()

    def reset(self):
        for p in self.__ps:
            p.kill()
            p.wait()

__all__.append("MemcachedHost")
class MemcachedHost(object):
    def __init__(self, host, dstPort, core = None, srcPort = None, threads = 1):
        self.host, self.dstPort, self.core, self.srcPort, self.threads = \
            host, dstPort, core, srcPort, threads

    def toInfoValue(self):
        return (self.host, self.dstPort, self.core, self.srcPort, self.threads)

    def getCommand(self, path, dstHost):
        cmd = [path]
        if self.core != None:
            cmd = ["numactl", "-C", str(self.core), "--"] + cmd
        if self.srcPort != None:
            cmd.extend(["-s", str(self.srcPort)])
        cmd.extend(["-p", str(self.dstPort),
                    self.host.routeToHost(dstHost), str(self.threads), "1"])
        return cmd

class MemcachedLoad(Task, ResultsProvider, SourceFileProvider):
    __info__ = ["mhosts", "counts", "drops", "*sysmonOut"]

    def __init__(self, mhosts, memcached, sysmon):
        Task.__init__(self)
        ResultsProvider.__init__(self, cores = len(memcached.ports))
        self.mhosts = mhosts
        self.memcached = memcached
        self.sysmon = sysmon

        self.__paths = dict((h, self.queueSrcFile(h, "mcload"))
                            for h in set(mhost.host for mhost in mhosts))
        self.__runners = dict((h, self.queueSrcFile(h, "run-mcload"))
                              for h in set(mhost.host for mhost in mhosts))

    def start(self):
        # Compile
        for host, path in self.__paths.iteritems():
            self.log("Building on %s" % host)
            host.r.run(["make", "-C", path, "mdc_udp"],
                       stdout = STDERR)

    def wait(self):
        cmds = [mh.getCommand(os.path.join(self.__paths[mh.host], "mdc_udp"),
                              self.memcached.host)
                for mh in self.mhosts]

        # Run.  These RPC's are synchronous, so we do them in parallel
        # to minimize skew.
        asyncs = []
        logs = []
        for n, cmd in enumerate(cmds):
            host = self.mhosts[n].host
            logPath = host.getLogPath(self) + "-%s.%d" % (host, n)
            cmd = [self.__runners[host], str(STARTTIME), str(SYNCTIME),
                   str(DURATION)] + cmd
            asyncs.append(Async(host.r.run, cmd, stdout = logPath,
                                wait = False))
            logs.append((host, logPath))
        ls = map(Async.sync, asyncs)

        # Wait for everything to start and enter its measurement period
        time.sleep(STARTTIME + SYNCTIME)

        # Start monitoring.  (XXX Do we need to time trigger this, too?)
        self.log("Starting monitoring")
        self.sysmon.startMonitor()

        # Wait for run duration
        self.log("Monitoring")
        time.sleep(DURATION)

        # Stop monitoring
        self.log("Stopping monitoring")
        self.sysmonOut = self.sysmon.stopMonitor()

        # Cleanup processes
        for l in ls:
            l.wait()

        # Get result
        self.counts = []
        self.drops = []
        for host, logPath in logs:
            log = host.r.readFile(logPath)
            ms = re.findall("(?m)^([0-9]+) requests, ([0-9]+) drops$", log)
            if len(ms) != 1:
                raise RuntimeError("Expected 1 result in log, got %d",
                                   len(ms))
            self.counts.append(int(ms[0][0]))
            self.drops.append(int(ms[0][1]))
        if sum(self.drops):
            self.log("Warning: %d dropped requests" % sum(self.drops))
        self.setResults(sum(self.counts), "request", "requests",
                        self.sysmonOut["time.real"])

class MemcachedRunner(object):
    def __str__(self):
        return "memcached"

    @staticmethod
    def run(m, cfg):
        ports = range(BASE_PORT, BASE_PORT + cfg.cores)
        host = cfg.primaryHost
        loadHosts = cfg.getMemcacheClients(host, ports)

        m += host
        m += HostInfo(host)
        for loadHost in set(h.host for h in loadHosts):
            m += loadHost

        if cfg.hotplug:
            m += SetCPUs(host = host, num = cfg.cores)
        # XXX Make configurable (at least iface name)
        m += IXGBE(host, "eth0", queues = "n%min(NCPU, NRX if rx else NTX)",
                   flowDirector = "fixed-port-routing")

        # We need to start memcached after hot-plugging cores because
        # we bind it to specific cores.
        mcPath = os.path.join(cfg.benchRoot, "memcached")
        memcached = Memcached(host, mcPath, "memcached-stock", ports, 1, 400)
        m += memcached

        sysmon = ExplicitSystemMonitor(host)
        m += sysmon
        m += MemcachedLoad(loadHosts, memcached, sysmon)
        m.run()

__all__.append("runner")
runner = MemcachedRunner()
