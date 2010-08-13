from mparts.configspace import ConfigSpace
from mparts.host import Host
from support import perfLocked

mk = ConfigSpace.mk

##################################################################
# Shared configuration
#

shared = ConfigSpace.unit()

# MOSBENCH involves three types of hosts:
# 1. The single primary host runs the benchmark applications.  It
#    should be a large multicore machine, preferably running the GUK
#    Linux kernel.  For the Apache and Memcached benchmarks, it should
#    have a very fast network connection to the load generators.
# 2. A set of secondary hosts act as load generators for server-client
#    benchmarks.  This list should not include the primary host.
# 3. The driver host runs the driver script, which coordinates the
#    benchmark programs and load generators on the primary and
#    secondary hosts.  It can be run from a primary or secondary host,
#    though doing so may perturb the results.  This host must have ssh
#    keys set up for passwordless access to all primary and secondary
#    hosts.
# Here we configure these hosts.  All of the host names provided here
# must work from all of the hosts (for example, don't use
# "localhost"), unless explicit routes are provided from every host.

# Use "cmdModifier = perfLocked" if you use the "perflock" script to
# prevent conflicting machine access.  You probably don't.
tom = Host("tom", cmdModifier = perfLocked)
josmp = Host("josmp")

shared *= mk(primaryHost = tom)
# Careful, mk treats a list as a set of alternate configurations, so
# you probably want to pass a list with a single element that is a
# list of secondary hosts.
# XXX
#shared *= mk(secondaryHosts = [[josmp]])

# For hosts have fast network links on alternate interfaces, specify
# the appropriate links here.  It's best if these are symmetrical.
def addRoute(host1, ip1, host2, ip2):
    # host1 has ip1 and host2 has ip2.
    host1.addRoute(host2, ip2)
    host2.addRoute(host1, ip1)
addRoute(tom, "192.168.42.11", josmp, "192.168.42.10")

# benchRoot specifies the directory on the primary host where MOSBENCH
# was checked out or unpacked.
shared *= mk(benchRoot = "~/mosbench")

# textRoot specifies the directory on the primary host where the text
# to use for the Psearchy indexing benchmark can be found.  (XXX other
# benchmarks?)  To reproduce the results in the paper, this should be
# a pristine check out of Linux 2.6.35-rc5 (XXX correct?).
shared *= mk(textRoot = "~/scale-linux")

# fs specifies which type of file system to use.  The file system
# names here must match the available file system types in mkmounts.
shared *= mk(fs = "tmpfs-separate")

# trials is the number of times to run each benchmark.  The best
# result will be taken.
shared *= mk(trials = 3)

# monitors specifies the set of monitoring tasks to run during each
# benchmark.  This can be overridden for individual benchmarks (though
# this will replace the list, not add to it).  See the note for
# secondaryHosts about lists.
# XXX
#shared *= mk(monitors = [[]])

# hotplug specifies whether or not to use CPU hotplug to physically
# disable cores not in use by the benchmark.  All cores should be
# re-enabled when the benchmark exits, even after an error.  Enabling
# this requires granting the benchmark user on the primary host sudo
# access.
shared *= mk(hotplug = True)

# cores specifies the number of cores to use.  This must be the last
# variable in the shared configuration for the graphing tools to work
# (which also means it generally can't be overridden per benchmark).
shared *= mk(cores = [1] + range(0, 49, 4)[1:])

##################################################################
# psearchy
#

import psearchy

psearchy = mk(benchmark = psearchy.runner, nonConst = True)

psearchy *= (mk(mode = ["thread"]) * mk(order = ["seq"]) +
             mk(mode = ["process"]) * mk(order = ["seq", "rr"]))
psearchy *= mk(mem = 1024)

##################################################################
# Metis
#

import metis

metis = mk(benchmark = metis.runner, nonConst = True)

metis *= mk(streamflow = True)
metis *= mk(model = ["default", "hugetlb"])

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
#configSpace = (psearchy + metis).merge(shared)
configSpace = (psearchy).merge(shared)
#configSpace = (metis).merge(shared)

##################################################################
# Run
#

if __name__ == "__main__":
    from mparts.manager import generateManagers
    from mparts.rpc import print_remote_exception
    import sys
    sys.excepthook = print_remote_exception
    for (m, cfg) in generateManagers("results", configSpace):
        cfg.benchmark.run(m, cfg)
