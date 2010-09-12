from mparts.manager import Task
from mparts.host import HostInfo, SourceFileProvider, STDERR
from support import ResultsProvider, SetCPUs, FileSystem, ExplicitSystemMonitor
import postgres

import os, time, signal, re

__all__ = []

__all__.append("PGLoad")
class PGLoad(Task, ResultsProvider, SourceFileProvider,
             postgres.PGOptsProvider):
    __config__ = ["host", "clients", "rows", "partitions", "*sysmonOut"]

    def __init__(self, host, pg, cores, clients, rows, partitions, sysmon):
        Task.__init__(self, host = host)
        ResultsProvider.__init__(self, cores)
        # XXX Use this elsewhere
        self.setConfigAttrs(PGLoad, locals())
        self.pg = pg
        self.sysmon = sysmon
        self.__dbname = "pg%d-%d" % (self.rows, self.partitions)

        self.__pgload = self.queueSrcFile(host, "pgload")
        self.queueSrcFile(host, "libload")

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

        # XXX Prefetch

    def wait(self):
        cmd = self.__cmd("bench")
        for arg in ["rows", "partitions", "clients"]:
            cmd.extend(["--" + arg, str(getattr(self, arg))])

        # Run
        logPath = self.host.getLogPath(self)
        l = self.host.r.run(cmd, stdout = logPath, wait = False)

        # Wait for warmup duration (XXX)
        time.sleep(3)

        # Start monitoring
        l.kill(signal.SIGUSR1)
        self.sysmon.startMonitor()

        # Wait for run duration (XXX)
        time.sleep(10)

        # Stop monitoring
        l.kill(signal.SIGUSR2)
        self.sysmonOut = self.sysmon.stopMonitor()

        # Cleanup pgload
        time.sleep(1)
        l.kill(signal.SIGINT)

        # Get result
        log = self.host.r.readFile(logPath)
        ms = re.findall("(?m)^\[SIG\] ([0-9]+) total queries", log)
        if len(ms) != 1:
            raise RuntimeError("Expected 1 query count in log, got %d",
                               len(ms))
        self.setResults(int(ms[-1]), "query", "queries",
                        self.sysmonOut["time.real"])

# XXX Use this naming convention elsewhere.  Put the nice name in the
# ResultsProvider for the graphs.
class PostgresRunner(object):
    def __str__(self):
        return "postgres"

    @staticmethod
    def run(m, cfg):
        host = cfg.primaryHost
        loadgen = cfg.secondaryHost

        m += host
        m += loadgen
        # XXX Creating the db takes time.  It would be great if we
        # didn't wipe it out between data points.  Maybe just set
        # clean to False and put the DB in a subdirectory.
        fs = FileSystem(host, cfg.fs, clean = False) #, clean = True)
        m += fs
        dbdir = fs.path + "0"
        pgPath = os.path.join(cfg.benchRoot, "postgres")
        # XXX Postgres build
        pg = postgres.Postgres(host, pgPath, "pg-sysv", dbdir)
        m += postgres.InitDB(host, pg).addTrust(loadgen)
        m += pg
        sysmon = ExplicitSystemMonitor(host)
        m += sysmon
        # XXX Options, trials
        m += PGLoad(loadgen, pg, cfg.cores, 48, 10000000, 0, sysmon)
        m.run()

__all__.append("runner")
runner = PostgresRunner()