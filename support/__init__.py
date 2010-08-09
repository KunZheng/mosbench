import sys, os

from mparts.manager import Task
from mparts.host import *

__all__ = ["IXGBE", "SetCPUs", "PrefetchList", "perfLocked"]

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

        # oprofile has a habit of panicking if you hot plug CPU's
        # under it
        self.host.sudo.run(["opcontrol", "--deinit"],
                           wait = UNCHECKED)

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

def perfLocked(host, cmdSsh, cmdSudo, cmdRun):
    """This is a host command modifier that takes a performance lock
    using 'perflock' while the remote RPC server is running."""

    if cmdSudo:
        # Hosts always make a regular connection before a root
        # connection and we wouldn't want to deadlock with ourselves.
        return cmdSsh + cmdSudo + cmdRun
    # XXX Shared lock option
    return cmdSsh + ["perflock"] + cmdSudo + cmdRun
