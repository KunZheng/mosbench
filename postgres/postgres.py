from __future__ import with_statement

import os, time, socket

from mparts.manager import Task
from mparts.host import CAPTURE, DISCARD, STDERR
from mparts.util import *

__all__ = ["InitDB", "Postgres", "PGVal", "PGOptsProvider"]

USE_DEBUG_LIBC = False

class InitDB(Task):
    __info__ = ["host", "dbdir"]

    def __init__(self, host, pg):
        dbdir = pg.dbdir
        Task.__init__(self, host = host, dbdir = dbdir)
        self.host = host
        self.dbdir = dbdir
        self.pg = pg
        self.trust = []

    def addTrust(self, host):
        route = self.host.routeToHost(host)
        # Postgres requires a CIDR address
        ai = socket.getaddrinfo(route, None, socket.AF_INET, socket.SOCK_STREAM)
        self.trust.append(ai[0][4][0] + "/32")
        return self

    def start(self):
        dbdir = self.dbdir

        # Is there already a database here?
        try:
            self.host.r.readFile(os.path.join(dbdir, "PG_VERSION"))
            # Yep.  All set.
            self.log("Database already exists")
            return
        except EnvironmentError:
            pass

        # Create the directory
        self.host.r.run(["mkdir", "-p", dbdir])

        # Initialize the database
        self.pg.initdb(dbdir)

        # Set up trust
        for t in self.trust:
            hba = "\nhost all all %s trust\n" % t
            self.host.r.writeFile(os.path.join(dbdir, "pg_hba.conf"),
                                  hba, noCheck = True, append = True)

class Postgres(Task):
    __info__ = ["host", "pgPath", "pgBuild", "dbdir", "malloc", "*dynOpts"]

    def __init__(self, host, pgPath, pgBuild, dbdir, malloc = "glibc", **opts):
        Task.__init__(self, host = host)
        self.host = host
        self.pgPath = pgPath
        self.pgBuild = pgBuild
        self.dbdir = dbdir
        self.malloc = malloc
        # For custom Postgres builds, we have to point it to its
        # libraries or it will try to use the system libraries.  At
        # best, it will probably have the wrong default socket path.
        self.__addEnv = {"LD_LIBRARY_PATH" : os.path.join(pgPath, pgBuild, "lib")}
        if USE_DEBUG_LIBC:
            self.__addEnv["LD_LIBRARY_PATH"] += ":/usr/lib/debug"
        if malloc == "glibc":
            pass
        elif malloc == "tcmalloc":
            self.__addEnv["LD_PRELOAD"] = "/usr/lib/libtcmalloc_minimal.so"
        else:
            raise ValueError("Unknown malloc %s" % malloc)
        self.__userOpts = opts
        self.dynOpts = {}

    def conninfo(self, fromHost, dbname = None):
        if fromHost == self.host:
            ci = []
        else:
            ci = ["host=%s" % (fromHost.routeToHost(self.host))]
        if dbname:
            ci.append("dbname=%s" % dbname)
        return " ".join(ci)

    def connargs(self, fromHost, dbname = None):
        if fromHost == self.host:
            ca = []
        else:
            ca = ["-h", fromHost.routeToHost(self.host)]
        if dbname:
            ca.extend(["-d", dbname])
        return ca

    def __bin(self, name):
        return os.path.join(self.pgPath, self.pgBuild, "bin", name)

    def initdb(self, dbdir):
        self.host.r.run([self.__bin("initdb"), dbdir], addEnv = self.__addEnv)

    def psql(self, sql, dbname = None, discard = False, args = ["-A", "-t"]):
        p = self.host.r.run([self.__bin("psql")] +
                            self.connargs(self.host, dbname = dbname) +
                            args + ["-c", sql],
                            stdout = DISCARD if discard else CAPTURE,
                            addEnv = self.__addEnv)
        if discard:
            return None
        return p.stdoutRead()

    def dbExists(self, name):
        res = self.psql("select count(1) from pg_catalog.pg_database "
                        "where datname = '%s'" % name,
                        dbname = "template1")
        return bool(int(res))

    def createDB(self, name):
        self.host.r.run([self.__bin("createdb"), name],
                        addEnv = self.__addEnv)

    def tableExists(self, name, dbname = None):
        res = self.psql("select count(1) from pg_tables "
                        "where schemaname = 'public' "
                        "and tablename = '%s'" % name,
                        dbname = dbname)
        return bool(int(res))

    def __getPostmaster(self):
        # Find the postmaster
        ps = self.host.r.procList()
        running = False
        for pid, info in ps.iteritems():
            if info["status"]["Name"] == "postgres":
                running = True
                if len(info["cmdline"]) and info["cmdline"][0].startswith("/"):
                    return pid
        if running:
            raise ValueError("Postgres is running, but failed to find postmaster")

    def __getAllOpts(self, m):
        # Set some defaults
        opts = {"datestyle": "iso, mdy",
                "lc_messages": "C",
                "lc_monetary": "C",
                "lc_numeric": "C",
                "lc_time": "C"}
        # Gather configuration from the manager
        providers = {}
        for p in m.find(cls = PGOptsProvider):
            for k, v in p.getPGOpts(self).iteritems():
                if k in providers and opts[k] != v:
                    raise ValueError("%s and %s differ on Postgres setting %s" %
                                     (providers[k], p, k))
                opts[k] = v
                providers[k] = p
        # Local configuration overrides
        opts.update(self.__userOpts)
        return opts

    def __optsToConffile(self, config):
        lines = []
        for k, v in config.iteritems():
            if isinstance(v, bool) or isinstance(v, int) or isinstance(v, float):
                vs = str(v)
            elif isinstance(v, str):
                vs = "'%s'" % v.replace("'", "''")
            elif isinstance(v, PGVal):
                vs = str(v)
            else:
                raise ValueError("Setting %s value %s has unknown %s",
                                 (k, v, type(v)))
            lines.append("%s = %s" % (k, vs))
        return "\n".join(lines)

    def __queryDynConfig(self):
        # Query the running configuration
        q = "select name, setting, unit, vartype from pg_settings"
        res = self.psql(q, dbname = "template1",
                        args = ["-A", "-P", "fieldsep=\t", "-P", "t"])
        config = {}
        for l in res.splitlines():
            # Parse row
            parts = l.split("\t")
            if len(parts) != 4:
                raise ValueError("Failed to parse pg_settings row %r" % l)
            name, setting, unit, vartype = parts

            # Parse value
            if vartype == "bool":
                value = {"off":False, "on":True}[setting]
            elif vartype == "string":
                value = setting
            elif vartype == "integer":
                value = int(setting)
            elif vartype == "real":
                value = float(setting)
            else:
                raise ValueError("Unknown vartype in pg_settings row %r" % l)

            # Parse unit
            if unit and (vartype == "integer" or vartype == "real"):
                if unit[0].isdigit():
                    value *= int(unit[0])
                    unit = unit[1:]
                value = PGVal(value, unit)
            elif unit:
                raise ValueError("Not expecting unit on pg_settings row %r" % l)

            config[name] = value

        # Get compilation options
        for configOpt in ["LDFLAGS", "CFLAGS"]:
            p = self.host.r.run([self.__bin("pg_config"),
                                 "--" + configOpt.lower()],
                                stdout = CAPTURE, addEnv = self.__addEnv)
            config[configOpt] = p.stdoutRead().rstrip("\n")

        return config

    def start(self, m):
        # Stop any running postgres
        self.reset()

        # Write configuration file
        configPath = os.path.join(self.host.outDir(), "%s.conf" % self.name)
        config = self.__getAllOpts(m)
        self.host.r.writeFile(configPath, self.__optsToConffile(config))

        # Start postgres
        logPath = self.host.getLogPath(self)
        for mayFail in [True, False]:
            self.host.r.run([self.__bin("pg_ctl"),
                             "-D", self.dbdir,
                             "-o", "--config_file=%s" % configPath,
                             "start"],
                            stdout = logPath, addEnv = self.__addEnv)

            # Monitor log file
            # XXX Use standard monitor function
            done = False
            for retry in range(20):
                time.sleep(0.5)
                log = self.host.r.readFile(logPath)
                if "database system is ready" in log:
                    done = True
                    break
                if "FATAL" in log:
                    break
            else:
                raise RuntimeError("Timeout waiting for postgres to start")
            if done:
                break

            # Start up failed.  Make sure postmaster has exited.
            for retry in range(10):
                time.sleep(0.5)
                if not self.__getPostmaster():
                    break
            else:
                raise ValueError("Postgres failed to log start, but hasn't exited")

            # Is it a shmmax problem?
            if mayFail:
                log = self.host.r.readFile(logPath)
                if "could not create shared memory segment" in log:
                    req = int(log.split("size=", 1)[1].split(",", 1)[0])
                    with Progress("Raising SHMMAX to %d" % req):
                        self.host.sysctl("kernel.shmmax", req)
                    # Clear the log so we don't get tripped up by
                    # errors in it
                    self.host.r.writeFile(logPath, "")
                    continue

            # Give up
            raise ValueError("Failed to start postgres")

    def stop(self):
        # Get full postgres configuration
        self.dynOpts = self.__queryDynConfig()
        # We don't actively stop the server.  It's not doing (much)
        # harm, and this way we might be able to reuse it
        # XXX Stop the server.

    def reset(self):
        self.dynOpts = {}

        # Find the postmaster
        pid = self.__getPostmaster()
        if not pid:
            return

        # Signal the postmaster to do a fast exit
        with Progress("Shutting down postmaster %d" % pid):
            import signal
            self.host.r.kill(pid, signal.SIGINT)
            for retry in range(10):
                time.sleep(0.5)
                ps = self.host.r.procList()
                if pid not in ps:
                    return
            raise ValueError("Failed to cleanly shut down postmaster")

class PGVal(object):
    def __init__(self, val, unit):
        if unit not in ["kB", "MB", "GB", "ms", "s", "min", "h", "d"]:
            raise ValueError("Illegal Postgres unit %r" % unit)
        self.val = val
        self.unit = unit

    def __repr__(self):
        return "PGVal(%r, %r)" % (self.val, self.unit)

    def __str__(self):
        return "%d%s" % (self.val, self.unit)

    def __eq__(self, other):
        return (isinstance(other, PGVal) and
                self.val == other.val and self.unit == other.unit)

    def toInfoValue(self):
        return (self.val, self.unit)

class PGOptsProvider(object):
    def getPGOpts(self, pg):
        raise NotImplementedError("getPGOpts is abstract")
