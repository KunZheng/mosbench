import sys, os, signal, time

from mparts.manager import Task
from mparts.host import *

__all__ = []

__all__.append("ResultsProvider")
class ResultsProvider(object):
    __config__ = ["cores", "result", "units"]

    def __init__(self, cores):
        self.cores = cores

    def setResults(self, result, units):
        self.result = result
        self.units = units

__all__.append("BenchmarkRunner")
class BenchmarkRunner(ResultsProvider):
    __config__ = ["trials"]

    def __init__(self, cores, trials):
        ResultsProvider.__init__(self, cores)
        self.trials = trials
        assert hasattr(self, "runTrial")

    def wait(self, m):
        best = None
        for t in range(self.trials):
            self.log("Trial %d/%d" % (t+1, self.trials))
            result, units = self.runTrial(m, t)
            self.log("=> %s %s" % (result, units))
            if best == None:
                best = result, units
            else:
                if units != best[1]:
                    raise ValueError("Units changed between trials")
                best = (max(best[0], result), units)
        self.setResults(*best)

__all__.append("IXGBE")
class IXGBE(Task, SourceFileProvider):
    __config__ = ["host", "iface", "queues"]

    def __init__(self, host, iface, queues):
        Task.__init__(self, host = host, iface = iface)
        self.host = host
        self.iface = iface
        self.queues = queues

        self.__script = self.queueSrcFile(host, "ixgbe-set-affinity")

    def start(self):
        self.log("Setting %s queue affinity to %s" % (self.iface, self.queues))
        self.host.sudo.run([self.__script, self.iface, self.queues],
                           stdout = self.host.getLogPath(self))

    def stop(self):
        self.reset()

    def reset(self):
        self.host.sudo.run([self.__script, self.iface, "CPUS"],
                           stdout = self.host.getLogPath(self))
        self.log("Reset IXGBE IRQ masks to all CPUs")

CPU_CACHE = {}

__all__.append("SetCPUs")
class SetCPUs(Task, SourceFileProvider):
    __config__ = ["host", "num", "seq"]

    def __init__(self, host, num, seq = "seq"):
        Task.__init__(self, host = host)
        self.host = host
        self.num = num
        self.seq = seq

        self.__script = self.queueSrcFile(host, "set-cpus")
        self.__cpuSeq = self.queueSrcFile(host, "cpu-sequences")

    def start(self):
        # oprofile has a habit of panicking if you hot plug CPU's
        # under it
        self.host.sudo.run(["opcontrol", "--deinit"],
                           wait = UNCHECKED)

        if self.host not in CPU_CACHE:
            # Make sure all CPU's are online
            self.host.sudo.run([self.__script, "-i"], stdin = DISCARD)

            # Get CPU sequences
            p = self.host.r.run([self.__cpuSeq], stdout = CAPTURE)
            seqs = {}
            for l in p.stdoutRead().splitlines():
                name, cpus = l.strip().split(" ", 1)
                seqs[name] = map(int, cpus.split(","))

            # Start an interactive set-cpus.  We don't actually use
            # this, but when we disconnect from the host, the EOF to
            # this will cause it to re-enable all CPUs.  This way we
            # don't have to online all of the CPU's between each
            # experiment.  We'll do our best to online all of the
            # CPU's at the end before exiting, but even if we die a
            # horrible death, hopefully this will online everything.
            CPU_CACHE[self.host] = \
                (self.host.sudo.run([self.__script, "-i"],
                                    stdin = CAPTURE, wait = None),
                 seqs)
        else:
            seqs = CPU_CACHE[self.host][1]

        try:
            seq = seqs[self.seq]
        except KeyError:
            raise ValueError("Unknown CPU sequence %r" % self.seq)
        if len(seq) < self.num:
            raise ValueError("Requested %d cores, but only %d are available" %
                             (self.num, len(seq)))

        cmd = [self.__script, ",".join(map(str, seq[:self.num]))]
        self.host.sudo.run(cmd, wait = CHECKED)

    def reset(self):
        # Synchronously re-enable all CPU's
        if self.host not in CPU_CACHE:
            return

        sc = CPU_CACHE[self.host][0]
        sc.stdinClose()
        sc.wait()

PREFETCH_CACHE = set()

__all__.append("PrefetchList")
class PrefetchList(Task, SourceFileProvider):
    __config__ = ["host", "filesPath"]

    def __init__(self, host, filesPath, reuse = False):
        Task.__init__(self, host = host, filesPath = filesPath)
        self.host = host
        self.filesPath = filesPath
        self.reuse = reuse

        self.__script = self.queueSrcFile(host, "prefetch")

    def start(self):
        if self.reuse:
            if (self.host, self.filesPath) in PREFETCH_CACHE:
                return
            PREFETCH_CACHE.add((self.host, self.filesPath))

        self.host.r.run([self.__script, "-l"], stdin = self.filesPath)

__all__.append("FileSystem")
class FileSystem(Task, SourceFileProvider):
    __config__ = ["host", "fstype"]

    def __init__(self, host, fstype, clean = True):
        Task.__init__(self, host = host, fstype = fstype)
        self.host = host
        self.fstype = fstype
        self.clean = clean
        assert '/' not in fstype
        self.path = "/tmp/mosbench/%s/" % fstype
        if clean:
            self.__script = self.queueSrcFile(host, "cleanfs")

    def start(self):
        # Check that the file system exists.  We check the mount table
        # instead of just the directory so we don't get tripped up by
        # stale mount point directories.
        mountCheck = self.path.rstrip("/")
        for l in self.host.r.readFile("/proc/self/mounts").splitlines():
            if l.split()[1].startswith(mountCheck):
                break
        else:
            raise ValueError(
                "No file system mounted at %s.  Did you run 'mkmounts %s' on %s?" %
                (mountCheck, self.fstype, self.host))

        # Clean
        if self.clean:
            self.host.r.run([self.__script, self.fstype])

def waitForLog(host, logPath, name, secs, string):
    for retry in range(secs*2):
        log = host.r.readFile(logPath)
        if string in log:
            return
        time.sleep(0.5)
    raise RuntimeError("Timeout waiting for %s to start" % name)

class TimesProvider(object):
    pass

# XXX Perhaps this shouldn't be a task at all.  It has to send a
# source file, but doesn't have any life-cycle.  It might be more
# natural to record the results in the benchmark that's running this,
# especially if I wind up creating a copy of the benchmark runner for
# each trial.
__all__.append("SystemMonitor")
class SystemMonitor(Task, SourceFileProvider, TimesProvider):
    __config__ = ["host", "times"]

    def __init__(self, host):
        Task.__init__(self, host = host)
        self.host = host
        self.times = []
        self.__script = self.queueSrcFile(host, "sysmon")

    def wrap(self, cmd, match = None):
        if match != None:
            return [self.__script, "-m", match] + cmd
        return [self.__script] + cmd

    def parseLog(self, log):
        mine = [l for l in log.splitlines() if l.startswith("[TimeMonitor] ")]
        if len(mine) == 0:
            raise ValueError("No sysmon report found in log file")
        if len(mine) > 1:
            raise ValueError("Multiple sysmon reports found in log file")

        parts = mine[0].split()[1:]
        self.times.append({})
        while parts:
            k, v = parts.pop(0), parts.pop(0)
            self.times[-1][k] = float(v)

__all__.append("perfLocked")
def perfLocked(host, cmdSsh, cmdSudo, cmdRun):
    """This is a host command modifier that takes a performance lock
    using 'perflock' while the remote RPC server is running."""

    if cmdSudo:
        # Hosts always make a regular connection before a root
        # connection and we wouldn't want to deadlock with ourselves.
        return cmdSsh + cmdSudo + cmdRun
    # XXX Shared lock option
    return cmdSsh + ["perflock"] + cmdSudo + cmdRun
