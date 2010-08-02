import sys, os

from mparts.manager import Task
from mparts.host import *

__all__ = ["IXGBE", "SetCPUs", "perfLocked"]

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
    __config__ = ["host", "num", "cpus"]

    def __init__(self, host, num = None, cpus = None):
        if num == None and cpus == None:
            raise ValueError("Either num or cpus must be specified")
        if num != None and cpus != None:
            raise ValueError("Only one of num or cpus must be specified")

        Task.__init__(self, host = host)
        self.host = host
        if num != None:
            cpus = range(0, num)
        else:
            num = len(cpus)
        self.num = num
        self.cpus = cpus

        self.__script = self.queueSrcFile(host, "set-cpus")

    def start(self):
        if self.host not in CPU_CACHE:
            # Start an interactive set-cpus.  We don't actually use
            # this, but when we disconnect from the host, the EOF to
            # this will cause it to re-enable all CPUs.  This way we
            # don't have to online all of the CPU's between each
            # experiment.
            #
            # XXX Will it be onlining CPU's without the perflock?
            CPU_CACHE[self.host] = \
                self.host.sudo.run([self.__script, "-i"],
                                   stdin = CAPTURE, wait = None)

        # oprofile has a habit of panicking if you hot plug CPU's
        # under it
        self.host.sudo.run(["opcontrol", "--deinit"],
                           wait = UNCHECKED)

        cmd = [self.__script, ",".join(map(str, self.cpus))]
        self.host.sudo.run(cmd, wait = CHECKED)

    def reset(self):
        # Synchronously re-enable all CPU's
        if self.host not in CPU_CACHE:
            return

        sc = CPU_CACHE[self.host]
        sc.stdinClose()
        sc.wait()

def perfLocked(host, cmdSsh, cmdSudo, cmdRun):
    """This is a host command modifier that takes a performance lock
    using 'perflock' while the remote RPC server is running."""

    if cmdSudo:
        # Hosts always make a regular connection before a root
        # connection and we wouldn't want to deadlock with ourselves.
        return cmdSsh + cmdSudo + cmdRun
    # XXX Shared lock option
    return cmdSsh + ["perflock"] + cmdSudo + cmdRun
