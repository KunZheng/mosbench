# MOSBENCH involves three types of hosts:
# 1. The single primary host runs the benchmark applications.  It
#    should be a large multicore machine.  For the Apache and
#    Memcached benchmarks, it should have a very fast network
#    connection to the load generators.
# 2. A set of client machines act as load generators for the
#    memcached, Apache, and Postgres benchmarks.  This list should
#    *not* include the primary host.
# 3. The driver host runs the driver script, which coordinates the
#    benchmark programs and load generators on the primary and
#    secondary hosts and gather results.  The benchmark was designed
#    so that this can be a different host from the primary host;
#    however, this is NOT a well-tested configuration.  This host must
#    have ssh keys set up for passwordless access to all hosts except
#    the one it is running on.
# Here we configure these hosts.  All of the host names provided here
# must work from all of the hosts.  Don't use "localhost".  The driver
# will detect if it is running on one of these hosts and forgo ssh
# automatically).

__all__ = ["primaryHost", "postgresClient", "getMemcacheClients",
           "getApacheClients"]

from mparts.host import Host
from support import perfLocked
from memcached import MemcachedHost
import support.rsshash as rsshash

# Use "cmdModifier = perfLocked" if you use the "perflock" script to
# prevent conflicting machine access.  You probably don't.
tom = Host("tom.csail.mit.edu", cmdModifier = perfLocked)

clientHosts = ["josmp.csail.mit.edu",
               "oc-5.csail.mit.edu", "oc-6.csail.mit.edu",
               "oc-7.csail.mit.edu", "oc-8.csail.mit.edu",
               "oc-9.csail.mit.edu", "oc-10.csail.mit.edu",
               "oc-11.csail.mit.edu",
               "brick-1.csail.mit.edu", "brick-2.csail.mit.edu",
               "brick-3.csail.mit.edu", "brick-4.csail.mit.edu",
               "brick-5.lcs.mit.edu", "brick-6.lcs.mit.edu",
               "brick-7.lcs.mit.edu", "brick-8.lcs.mit.edu",
               "brick-9.lcs.mit.edu", "brick-10.lcs.mit.edu",
               "brick-11.lcs.mit.edu", "brick-12.lcs.mit.edu",
               "brick-14.lcs.mit.edu",
               "hooverdam.lcs.mit.edu",
               "ud0.csail.mit.edu",
               "ud1.csail.mit.edu",
               "hydra.lcs.mit.edu"]
clients = dict((hostname.split(".",1)[0], Host(hostname))
               for hostname in clientHosts)

# josmp is special
josmp = clients["josmp"]

# All clients are on the 10GB switch, so they should address tom using
# its 10GB IP.
for host in clients.values():
    host.addRoute(tom, "192.168.42.11")

# josmp is multihomed and uses a different IP for its 10GB interface.
tom.addRoute(josmp, "192.168.42.10")

# tom is our big multicore
primaryHost = tom

# Postgres uses a single client load generator, which should be a
# reasonably beefy machine (we use a 16 core machine, though come
# nowhere near its capacity).  The ratio of client work to server work
# for Postgres is nowhere near that of Apache or memcached.  This host
# must have the Postgres client library installed (libpq-dev on
# Debian/Ubuntu).
postgresClient = clients["josmp"]

# Memcached uses a large set of clients, very carefully chosen to
# maximize load.  It turns out we have just enough clients to hit the
# limits of tom's IXGBE.
def getMemcacheClients(dstHost, ports):
    lst = []

    # For each memcache port, dedicate two threads from josmp.  josmp
    # has 16 cores and an IXGBE, so to get peak performance out of it,
    # we have to carefully assign source ports so the reply packets go
    # to right cores.  josmp is configured to use RSS, so we reverse
    # the RSS hash to construct these source ports.
    pg = rsshash.PortGenerator(16, 8000)
    josmpCore = 0
    for port in ports*2:
        # Assume that queue N is assigned to josmp core N
        srcPort = pg.genIPv4UDP(dstHost.routeToHost(josmp), port,
                                josmp.routeToHost(dstHost), josmpCore)
        lst.append(MemcachedHost(josmp, port, core = josmpCore,
                                 srcPort = srcPort))
        josmpCore = (josmpCore + 1) % 16

    # For each memcache port, dedicate 15 threads from the other clients
    clientsRep = clients.values()
    clientsRep.remove(josmp)
    clientsRep = (clientsRep * len(ports))[:len(ports)]
    for port, client in zip(ports, clientsRep):
        lst.append(MemcachedHost(client, port, threads = 15))

    return lst

apacheList = ["josmp", "josmp", "josmp", "josmp",
              "josmp", "josmp", "josmp", "josmp",
              "josmp", "josmp", "josmp", "josmp",
              "josmp", "josmp", "josmp", "josmp",

              "brick-1", "brick-2", "brick-3", "brick-4",
              "brick-5", "brick-5", "brick-6", "brick-7",
              "brick-7", "brick-8", "brick-8", "brick-9",
              "brick-9", "brick-10", "brick-10", "brick-11",
              "brick-11", "brick-12", "brick-12", "brick-14",
              "brick-14",

              "oc-5", "oc-6", "oc-7", "oc-8",
              "oc-9", "oc-10", "oc-11",

              "ud0", "ud0", "ud0", "ud0",
              "hydra", "hydra", "hydra", "hydra",
              "ud1", "ud1", "ud1", "ud1",
              "hooverdam", "hooverdam"]

def getApacheClients(cfg):
    # Since our clients our rather heterogeneous, we fix the set of
    # clients and instead vary the connection rate.
    return [clients[h] for h in apacheList]
