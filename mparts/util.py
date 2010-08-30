import sys, os, errno, socket, select

class Progress(object):
    """A context manager that prints out progress messages before and
    after an action."""

    def __init__(self, msg, done = "done"):
        self.__msg = msg
        self.__done = done

    def __enter__(self):
        print >> sys.stderr, "%s..." % self.__msg

    def __exit__(self, typ, value, traceback):
        if typ == None:
            print >> sys.stderr, "%s... %s" % (self.__msg, self.__done)
        else:
            print >> sys.stderr, "%s... FAILED (%s)" % (self.__msg, value)

def maybeMakedirs(p):
    """Like os.makedirs, but it is not an error for the directory
    already to already exist."""

    try:
        os.makedirs(p)
    except EnvironmentError, e:
        if e.errno != errno.EEXIST:
            raise

def dictToCmdline(dct):
    args = []
    for k, v in dct.items():
        args.append("--%s=%s" % (k, v))
    return args

def relpath(path, start=os.path.curdir):
    """Return a relative version of a path.
    Lifted from Python 2.6 os.path."""

    if not path:
        raise ValueError("no path specified")

    start_list = os.path.abspath(start).split(os.path.sep)
    path_list = os.path.abspath(path).split(os.path.sep)

    # Work out how much of the filepath is shared by start and path.
    i = len(os.path.commonprefix([start_list, path_list]))

    rel_list = [os.path.pardir] * (len(start_list)-i) + path_list[i:]
    if not rel_list:
        return os.path.curdir
    return os.path.join(*rel_list)

def isLocalhost(host):
    # Listen on a random port
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("", 0))
    s.listen(1)
    s.setblocking(False)

    # Try to connect to that port on host.  This will come from a
    # random port.  For some reason, we can't use a 0 timeout, so we
    # use a really small one.
    try:
        c = socket.create_connection((host, s.getsockname()[1]), 0.01)
    except socket.error:
        return False

    # We're probably good, but accept the connection and make sure it
    # came from the right port.
    try:
        (a, _) = s.accept()
    except socket.error:
        return False
    return a.getpeername()[1] == c.getsockname()[1]
