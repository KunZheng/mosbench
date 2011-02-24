#!/usr/bin/python

import subprocess
import signal
import errno
import time
import sys
import os

def start_clients(count, execOp):
    procs = []
    for i in range(count):
        procs.append(
            subprocess.Popen(['o/exec-client', '-coreid', str(i),
                              '-exec_op', execOp],
                             stdout = subprocess.PIPE))
        time.sleep(0.05)
    return procs

def stop_clients(procs):
    os.kill(0, signal.SIGUSR2)
    total = 0.0
    for p in procs:
        res = p.stdout.readline()
        total += float(res.split()[1])
    sys.stdout.flush()
    return total

def timed_wait(timeout):
    expired = [False]
    def on_alarm(signum, frame):
        expired[0] = True
    signal.signal(signal.SIGALRM, on_alarm)
    signal.alarm(timeout)
    try:
        return os.wait()
    except OSError, e:
        if e.errno != errno.EINTR or not expired[0]:
            raise
        return (0, 0)
    finally:
        signal.alarm(0)

def usage():
    print >> sys.stderr, "Usage: %s time ncores exec_op" % sys.argv[0]
    sys.exit(2)

if len(sys.argv) < 4:
    usage()

duration = int(sys.argv[1])
ncores = int(sys.argv[2])
execOp = sys.argv[3]

signal.signal(signal.SIGUSR1, signal.SIG_IGN)
signal.signal(signal.SIGUSR2, signal.SIG_IGN)
# start all the clients
procs = start_clients(ncores, execOp)
time.sleep(1)
# reset the counts
os.kill(0, signal.SIGUSR1)
# wait for duration
timed_wait(duration)
# kill the clients and count up the results
total = stop_clients(procs)
print total
