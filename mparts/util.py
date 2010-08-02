import sys, os, errno

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
