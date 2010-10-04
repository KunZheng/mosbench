from __future__ import with_statement

from mparts.manager import Task
from mparts.host import HostInfo, SourceFileProvider, STDERR
from mparts.util import Progress
from support import ResultsProvider, SetCPUs, IXGBE, FileSystem, \
    ExplicitSystemMonitor
import postgres

import os, time, signal, re, math

__all__ = []

# XXX This warmup sometimes isn't enough.  Watch the log for when it's
# ready, then warmup.
WARMUP = 5
DURATION = 15

__all__.append("PostgresLoad")
class PostgresLoad(Task, ResultsProvider, SourceFileProvider,
             postgres.PGOptsProvider):
    __info__ = ["host", "trial", "clients", "rows", "partitions",
                "batchSize", "randomWritePct", "*sysmonOut"]

    def __init__(self, host, trial, pg, cores, clients, rows, partitions,
                 batchSize, randomWritePct, sysmon):
        Task.__init__(self, host = host, trial = trial)
        ResultsProvider.__init__(self, cores)
        # XXX Use this elsewhere
        self.setInfoAttrs(PostgresLoad, locals())
        self.pg = pg
        self.sysmon = sysmon
        self.__dbname = "pg%d-%d" % (self.rows, self.partitions)

        self.__pgload = self.queueSrcFile(host, "pgload")
        self.queueSrcFile(host, "libload")

        if trial != 0:
            # Only start one PostgresLoad object per data point
            self.start = None

    def getPGOpts(self, pg):
        return {"listen_addresses": "*"}

    def __cmd(self, *args):
        return [os.path.join(self.__pgload, "pgload"),
                "-p", self.pg.conninfo(self.host,
                                       dbname = self.__dbname)] + list(args)

    def start(self):
        # Build (We rely on the host's libpq here!  Without copying
        # all of Postgres over and building it, there's no way to
        # reliably get a proper libpq.  We can't even statically link
        # pgload locally and send that over because the glibc name
        # system broke static linking.  Besides, we're not picky about
        # our libpq.)
        self.host.r.run(["make", "-C", self.__pgload],
                        stdout = STDERR)

        # Construct table names
        if self.partitions == 0:
            tables = ["simplebench"]
        else:
            tables = ["simplebench%d" % n for n in range(self.partitions)]

        # Create database
        if not self.pg.dbExists(self.__dbname):
            self.pg.createDB(self.__dbname)

        # Populate database
        if not self.pg.tableExists(tables[0], self.__dbname):
            self.host.r.run(
                self.__cmd("create", "--rows=%d" % self.rows,
                           "--partitions=%d" % self.partitions),
                stdout = STDERR)

        # Prefetch
        with Progress("Prefetching tables"):
            for t in tables:
                self.pg.psql("select * from " + t, dbname = self.__dbname,
                             discard = True)
                self.pg.psql("vacuum " + t, dbname = self.__dbname,
                             discard = True)

    def wait(self):
        cmd = self.__cmd("bench")
        for arg in ["rows", "partitions", "clients", "batchSize",
                    "randomWritePct"]:
            cmd.extend(["--" + arg.lower(), str(getattr(self, arg))])

        # Run
        logPath = self.host.getLogPath(self)
        l = self.host.r.run(cmd, stdout = logPath, wait = False)

        # Wait for warmup duration
        time.sleep(WARMUP)

        # Start monitoring
        l.kill(signal.SIGUSR1)
        self.sysmon.startMonitor()

        # Check that pgload hasn't died on us, rather than find out at
        # the end of the run
        l.wait(poll = True)

        # Wait for run duration
        time.sleep(DURATION)

        # Stop monitoring
        l.kill(signal.SIGUSR2)
        self.sysmonOut = self.sysmon.stopMonitor()

        # Cleanup pgload
        time.sleep(1)
        l.kill(signal.SIGINT)
        l.wait()

        # Get result
        log = self.host.r.readFile(logPath)
        ms = re.findall("(?m)^\[SIG\] ([0-9]+) total queries", log)
        if len(ms) != 1:
            raise RuntimeError("Expected 1 query count in log, got %d",
                               len(ms))
        self.setResults(int(ms[-1]), "query", "queries",
                        self.sysmonOut["time.real"])

def getBuild(cfg):
    if cfg.sleep not in ["sysv", "posix"]:
        raise ValueError(
            "Postgres sleep mode must be sysv or posix, got %r" % cfg.sleep)
    build = "pg-%s" % cfg.sleep
    if cfg.lwScale:
        build += "-lwscale"
    if cfg.lockScale:
        if not cfg.lwScale:
            raise ValueError("lockscale requires lwscale")
        build += "-lockscale"
    return build

class PostgresRunner(object):
    def __str__(self):
        return "postgres"

    @staticmethod
    def run(m, cfg):
        host = cfg.primaryHost
        loadgen = cfg.postgresClient

        m += host
        m += loadgen
        m += HostInfo(host)
        # Creating the db takes time, so we don't clean the file
        # system.  We avoid any cruft that may be there already by
        # putting the DB in a subdirectory.
        fs = FileSystem(host, cfg.fs, clean = False)
        m += fs

        dbdir = fs.path + "0/postgres"
        pgPath = os.path.join(cfg.benchRoot, "postgres")
        pgBuild = getBuild(cfg)
        pgOpts = {"shared_buffers": postgres.PGVal(cfg.bufferCache, "MB")}
        log2NumLockPartitions = int(math.log(cfg.lockPartitions, 2))
        if cfg.lockPartitions != 1 << log2NumLockPartitions:
            raise ValueError("numLockPartitions must be a power of 2, got %r" %
                             cfg.numLockPartitions)
        pgOpts["log2_num_lock_partitions"] = log2NumLockPartitions
        if cfg.sleep == "sysv":
            pgOpts["semas_per_set"] = cfg.semasPerSet
        pg = postgres.Postgres(host, pgPath, pgBuild, dbdir,
                               malloc = cfg.malloc, **pgOpts)
        m += postgres.InitDB(host, pg).addTrust(loadgen)
        m += pg

        if cfg.hotplug:
            # Because the number of cores and the number of clients is
            # the same, we don't strictly need hotplug
            m += SetCPUs(host = host, num = cfg.cores)
        # XXX Make configurable (at least iface name)
#        m += IXGBE(host, "eth0", queues = "n*NCPU/(NRX if rx else NTX)")
        # The ixgbe driver assigns flows to queues sequentially.
        # Since we only have cfg.cores flows, make sure a sequential
        # assignment spans all the online cores.  However, this does
        # not spread things out if we have more queues than cores.
        m += IXGBE(host, "eth0", queues = "n%min(NCPU, NRX if rx else NTX)")

        sysmon = ExplicitSystemMonitor(host)
        m += sysmon
        for trial in range(cfg.trials):
            m += PostgresLoad(loadgen, trial, pg, cfg.cores, cfg.cores,
                              cfg.rows, cfg.partitions, cfg.batchSize,
                              cfg.randomWritePct, sysmon)
        m.run()

__all__.append("runner")
runner = PostgresRunner()
