from mparts.manager import Task
from mparts.host import HostInfo, SourceFileProvider, STDERR
from mparts.util import Async
from support import ResultsProvider, SetCPUs, SystemMonitor, IXGBE

import os, time, re

__all__ = []

WARMUP = 5
DURATION = 15
PORT = 8000

extraConf = """
<Directory />
    Options FollowSymLinks
    AllowOverride None
    Order deny,allow
    Deny from all
</Directory>

<Directory "%s">
    Options Indexes FollowSymLinks
    AllowOverride None
    Order allow,deny
    Allow from all
</Directory>

<Location /server-status>
    SetHandler server-status
    Allow from all
</Location>
ExtendedStatus On
"""

__all__.append("Apache")
class Apache(Task):
    __info__ = ["host", "apachePath", "apacheBuild", "port", "bytes", "*opts"]

    def __init__(self, host, apachePath, apacheBuild, port, bytes, **opts):
        Task.__init__(self, host = host)
        self.setInfoAttrs(Apache, locals())
        self.opts = opts
        self.__httpd = os.path.join(self.apachePath, self.apacheBuild,
                                    "bin", "httpd")
        self.__configPath = self.host.outDir("%s.conf" % self.name)

    def __optsToConffile(self, opts):
        lines = []
        for k, v in opts.iteritems():
            if isinstance(v, bool):
                vs = "On" if v else "Off"
            elif isinstance(v, (int, str)):
                vs = repr(v)
            else:
                raise ValueError("Setting %s value %s has unknown %s",
                                 (k, v, type(v)))
            lines.append("%s %s" % (k, v))
        return "\n".join(lines) + (extraConf % opts["DocumentRoot"])

    def start(self):
        self.__stopApache()

        # Create the file to serve
        docRoot = self.host.outDir("htdocs")
        self.host.r.writeFile(os.path.join(docRoot, "x.txt"),
                              "x" * self.bytes, noCheck = True)

        # Construct config file
        opts = self.opts.copy()
        opts["Listen"] = self.port
        opts["DocumentRoot"] = docRoot
        opts["ErrorLog"] = self.host.getLogPath(self)
        opts["LogLevel"] = "warn"
        opts["PidFile"] = self.host.outDir("%s.pid" % self.name)
        self.host.r.writeFile(self.__configPath, self.__optsToConffile(opts))

        # Create Error log so that Apache doesn't complain
        self.host.r.writeFile(opts["ErrorLog"], "", append=True)

        # Start Apache.  Conveniently, by the time this returns the
        # server is listening.
        self.host.r.run([self.__httpd, "-f", self.__configPath, "-k", "start"])

    def stop(self):
        self.__stopApache()

    def reset(self):
        self.__stopApache()

    def __stopApache(self):
        if not self.__apachePIDs():
            return
        self.host.r.run([self.__httpd, "-f", self.__configPath, "-k", "stop"])
        pids = []
        for retry in range(20):
            time.sleep(0.5)
            pids = self.__apachePIDs()
            if not pids:
                break
        else:
            raise RuntimeError("Timeout waiting for Apache to shutdown; "
                               "still running with PIDs %s" % pids)

    def __apachePIDs(self):
        ps = self.host.r.procList()
        pids = []
        # Path expansion makes it hard to check for an exact "exe", so
        # just look at the end
        end = os.path.join(self.apacheBuild, "bin", "httpd")
        for pid, info in ps.iteritems():
            # XXX This wouldn't work if we had more than one Apache variant
            if info["exe"].endswith(end):
                pids.append(pid)
        return pids

# Unlike other benchmarks with load generators, we get the request
# count straight from Apache
class ApacheMon(Task, ResultsProvider, SourceFileProvider):
    __info__ = ["host", "*sysmonOut"]

    def __init__(self, apache, cores, sysmon):
        Task.__init__(self, host = apache.host)
        ResultsProvider.__init__(self, cores)
        self.apache = apache
        self.host = apache.host
        self.sysmon = sysmon
        self.__mon = self.queueSrcFile(self.host, "mon-apache")

    def wait(self):
        cmd = [self.__mon, str(WARMUP), str(DURATION), str(self.apache.port)]
        cmd = self.sysmon.wrap(cmd, "Starting", "Stopped")

        # Run
        logPath = self.host.getLogPath(self)
        self.host.r.run(cmd, stdout = logPath)

        # Get result
        log = self.host.r.readFile(logPath)
        self.sysmonOut = self.sysmon.parseLog(log)
        ms = re.findall("(?m)^([0-9]+) requests", log)
        if len(ms) != 1:
            raise RuntimeError("Expected 1 request count in log, got %d" %
                               len(ms))
        self.setResults(int(ms[0]), "request", "requests",
                        self.sysmonOut["time.real"])

class Httperf(Task, SourceFileProvider):
    __info__ = ["hosts", "rate", "fdlim"]

    def __init__(self, hosts, rate, apache, fdlim):
        Task.__init__(self)
        self.setInfoAttrs(Httperf, locals())
        self.apache = apache
        self.__ps = []
        self.__wrappers = dict((h, self.queueSrcFile(h, "withfdlim"))
                               for h in set(hosts))

    def start(self):
        asyncs = []
        for n, host in enumerate(self.hosts):
            # XXX --client?
            cmd = [self.__wrappers[host], str(self.fdlim),
                   "httperf",
                   "--timeout=5", "--client=0/1",
                   "--server=%s" % host.routeToHost(self.apache.host),
                   "--port=%d" % self.apache.port,
                   "--uri=/x.txt",
                   "--rate=%d" % self.rate,
                   "--send-buffer=4096", "--recv-buffer=16384",
                   "--num-conns=%d" % (self.rate*(DURATION+WARMUP+10)),
                   "--num-calls=1"]
            self.log("Starting %s" % host)
            logPath = host.getLogPath(self) + "-%s.%d" % (host, n)
            asyncs.append(Async(host.r.run, cmd, stdout = logPath,
                                wait = False))
            time.sleep(0.1)
        self.log("Syncing")
        self.__ps = map(Async.sync, asyncs)

    def stop(self):
        self.reset()

    def reset(self):
        for p in self.__ps:
            p.kill()
            p.wait()

class ApacheRunner(object):
    def __str__(self):
        return "apache"

    @staticmethod
    def run(m, cfg):
        host = cfg.primaryHost
        loadHosts = cfg.getApacheClients(cfg)

        m += host
        m += HostInfo(host)
        for loadHost in set(loadHosts):
            m += loadHost

        if cfg.hotplug:
            m += SetCPUs(host = host, num = cfg.cores)
        # XXX Make configurable (at least iface name)
        m += IXGBE(host, "eth0", queues = "n%min(NCPU, NRX if rx else NTX)",
                   flowDirector = "spread-incoming")

        # We need to start memcached after hot-plugging cores because
        # we bind it to specific cores.
        apachePath = os.path.join(cfg.benchRoot, "apache")
        total = cfg.cores * cfg.threadsPerCore
        apache = Apache(host, apachePath, "apache-mod", PORT, cfg.fileSize,
                        ListenBacklog = 512,
                        KeepAlive = False,
                        ServerLimit = cfg.cores,
                        StartServers = cfg.cores,
                        ThreadLimit = cfg.threadsPerCore,
                        MaxClients = total,
                        MinSpareThreads = total,
                        MaxSpareThreads = total,
                        ThreadsPerChild = cfg.threadsPerCore,
                        MaxRequestsPerChild = 0,
                        )
        m += apache

        m += Httperf(loadHosts, cfg.getApacheRate(cfg), apache,
                     cfg.getApacheFDLim(cfg))

        sysmon = SystemMonitor(host)
        m += sysmon

        m += ApacheMon(apache, cfg.cores, sysmon)

        m.run()

__all__.append("runner")
runner = ApacheRunner()
