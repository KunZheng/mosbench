from mparts.configspace import ConfigSpace

import hosts

# If set to True, do as few experiments as quickly as possible to test
# the setup.  This is useful to do before the full benchmark suite
# because it will almost certainly uncover misconfigurations that
# could halt a lengthy full benchmark part way through.
sanityRun = True

# For an explanation of configuration spaces and a description of why
# we use '*' and '+' all over this file, see the module documentation
# for mparts.configspace.  In short, * combines configuration options,
# while + specifies alternate configurations.  Likewise, passing a
# list to mk creates a set of alternate configurations.

mk = ConfigSpace.mk

##################################################################
# Shared configuration
#

shared = ConfigSpace.unit()

# The primary host that will run the benchmark applications.
shared *= mk(primaryHost = hosts.primaryHost)

# benchRoot specifies the directory on the primary host where MOSBENCH
# was checked out or unpacked.
shared *= mk(benchRoot = "~/mosbench")

# textRoot specifies the directory on the primary host where the text
# to use for the Psearchy indexing benchmark can be found.  To
# reproduce the results in the paper, this should be a pristine check
# out of Linux 2.6.35-rc5.
shared *= mk(textRoot = "~/scale-linux")

# kernelRoot specifies the directory on the primary host where the
# kernel source to use for the gmake benchmark can be found.  To
# reproduce the results in the paper, this should be a check out of
# Linux 2.6.35-rc5.  This can be the same directory used for textRoot
# above.
shared *= mk(kernelRoot = "~/scale-linux")

# fs specifies which type of file system to use.  This can be any file
# system type known to mkmounts except hugetlbfs.
shared *= mk(fs = "tmpfs-separate")

# trials is the number of times to run each benchmark.  The best
# result will be taken.
if sanityRun:
    shared *= mk(trials = 1)
else:
    shared *= mk(trials = 3)

# hotplug specifies whether or not to use CPU hotplug to physically
# disable cores not in use by the benchmark.  All cores should be
# re-enabled when the benchmark exits, even after an error.  Several
# of the benchmarks do not otherwise restrict which cores they use,
# and thus will give bogus results without this.
shared *= mk(hotplug = True)

# cores specifies the number of cores to use.  This must be
# non-constant and must be the last variable in the shared
# configuration for the graphing tools to work (which also means it
# generally shouldn't be overridden per benchmark).
if sanityRun:
    shared *= mk(cores = [48], nonConst = True)
else:
    shared *= mk(cores = [1] + range(0, 49, 4)[1:], nonConst = True)

##################################################################
# Exim
#
# eximBuild - The build name of Exim to run.  Corresponds to a
# subdirectory of the exim/ directory that contains an Exim
# installation.
#
# eximPort - The port Exim should listen on.
#
# clients - The number of client load generators to run.

import exim

exim = mk(benchmark = exim.runner, nonConst = True)

exim *= mk(eximBuild = "exim-mod")
exim *= mk(eximPort = 2526)
exim *= mk(clients = 96)

##################################################################
# memcached
#
# getMemcacheClients - A function that takes a destination host and a
# list of ports and returns a list of memcached.MemcachedHost objects
# to use as client load generators.

import memcached

memcached = mk(benchmark = memcached.runner, nonConst = True)

memcached *= mk(getMemcacheClients = hosts.getMemcacheClients)

##################################################################
# Apache
#
# threadsPerCore - The number of Apache threads to run per core.
#
# fileSize - The size of the file to serve, in bytes.
#
# getApacheClients - A function that, given the configuration, returns
# a list of Host objects that should be used for client load
# generators.  The same host may be returned multiple times.
#
# getApacheRate - A function that, given the configuration, returns
# the number of connections that each load generator client should
# attempt per second.
#
# getApacheFDLim - A function that, given the configuration, returns
# the FD limit for each load generator client.  This, in turn, limits
# the number of open connections each client can maintain at once.

import apache

apache = mk(benchmark = apache.runner, nonConst = True)

apache *= mk(threadsPerCore = 24)
apache *= mk(fileSize = 300)
apache *= mk(getApacheClients = hosts.getApacheClients)
apache *= mk(getApacheRate = lambda cfg: 100 + 400*cfg.cores)
apache *= mk(getApacheFDLim = lambda cfg: max(41 * cfg.cores / 20, 10))

##################################################################
# Postgres
#
# rows - The number of rows in the database.
#
# partitions - The number of tables to split the database across.
#
# batchSize - The number of queries each client should send to
# Postgres at a time.  This causes the load generator to act like a
# connection pooler with query aggregation.
#
# randomWritePct - The percentage of queries that should be updates.
#
# sleep - The method Postgres uses to sleep when a lock is taken.  Can
# be "sysv" for SysV semaphores or "posix" for POSIX semaphores (that
# is, futexes on Linux).
#
# semasPerSet - For sysv sleep, the number of semaphores per SysV
# semaphore set.  In the kernel, each semaphore set is protected by
# one lock.  Ignored for posix sleep.
#
# lwScale - Whether or not to use scalable lightweight locks
# (read/write mutexes) in Postgres.
#
# lockScale - Whether or not to use scalable database locks in
# Postgres.  Enabling scalable database locks requires scalable
# lightweight locks.
#
# lockPartitions - The number of partitions for the database lock
# manager.  Each partition is protected by an underlying lightweight
# lock.  This must be a power of 2.  The Postgres default is 1<<4.
#
# malloc - The malloc implementation to use in Postgres.  Must be
# tcmalloc or glibc.  For tcmalloc, you'll need to install the
# tcmalloc library.
#
# bufferCache - The size of the Postgres buffer cache, in megabytes.

import postgres

postgres = mk(benchmark = postgres.runner, nonConst = True)

postgres *= mk(postgresClient = hosts.postgresClient)

postgres *= mk(rows = 10000000)
postgres *= mk(partitions = 0)
postgres *= mk(batchSize = 256)
if sanityRun:
    postgres *= mk(randomWritePct = [5])
else:
    postgres *= mk(randomWritePct = [0, 5])

pgopt = (mk(sleep = "sysv") * mk(semasPerSet = 16) *
         mk(lwScale = True) * mk(lockScale = True) *
         mk(lockPartitions = 1<<10))
pgstock = (mk(sleep = "sysv") * mk(semasPerSet = 16) *
           mk(lwScale = False) * mk(lockScale = False) *
           mk(lockPartitions = 1<<4))

postgres *= pgopt + pgstock
postgres *= mk(malloc = "tcmalloc")
postgres *= mk(bufferCache = 2048)

##################################################################
# gmake
#

import gmake

gmake = mk(benchmark = gmake.runner, nonConst = True)

##################################################################
# psearchy
#
# mode - The mode to run mkdb in.  Must be "thread" or "process".
#
# seq - The sequence to assign cores in.  Must be "seq" for sequential
# assignment or "rr" for round-robin assignment.
#
# mem - How much memory to allocate to the hash table on each core, in
# megabytes.
#
# dblim - The maximum number of entries to store per Berkeley DB file.
# None for no limit.

import psearchy

psearchy = mk(benchmark = psearchy.runner, nonConst = True)

if sanityRun:
    psearchy *= (mk(mode = ["thread"]) * mk(order = ["seq"]) +
                 mk(mode = ["process"]) * mk(order = ["rr"]))
else:
    psearchy *= (mk(mode = ["thread"]) * mk(order = ["seq"]) +
                 mk(mode = ["process"]) * mk(order = ["seq", "rr"]))
psearchy *= mk(mem = 48)
psearchy *= mk(dblim = 200000)

##################################################################
# Metis
#
# streamflow - Whether or not to use the Streamflow parallel
# allocator.
#
# model - The memory allocation model to use.  Either "default" to use
# 4K pages or "hugetlb" to 2M pages.  "hugetlb" requires the
# Streamflow allocator.
#
# order - The sequence to assign cores in.  "seq" or "rr".

import metis

metis = mk(benchmark = metis.runner, nonConst = True)

metis *= mk(streamflow = True)
metis *= mk(model = ["hugetlb", "default"])
metis *= mk(order = ["rr"])

##################################################################
# Complete configuration
#

# XXX Hmm.  Constant analysis is space-global right now, so combining
# spaces for different benchmarks may give odd results.

# We compute the product of the benchmark configurations with the
# shared configuration instead of the other way around so that we will
# perform all configurations of a given benchmark before moving on to
# the next, even if the shared configuration space contains more than
# one configuration.  Furthermore, instead of computing the regular
# product, we compute a "merge" product, where assignments from the
# left will override assignments to the same variables from the right.
configSpace = ((exim + memcached + apache + postgres + gmake + psearchy + metis)
               .merge(shared))
#configSpace = exim.merge(shared)
#configSpace = memcached.merge(shared)
#configSpace = apache.merge(shared)
#configSpace = postgres.merge(shared)
#configSpace = gmake.merge(shared)
#configSpace = psearchy.merge(shared)
#configSpace = metis.merge(shared)

##################################################################
# Run
#

if __name__ == "__main__":
    from mparts.manager import generateManagers
    from mparts.rpc import print_remote_exception
    import sys
    sys.excepthook = print_remote_exception
    for (m, cfg) in generateManagers("sanity" if sanityRun else "results", configSpace):
        cfg.benchmark.run(m, cfg)
