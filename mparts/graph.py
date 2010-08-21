import os, subprocess, tempfile, re

__all__ = ["Gnuplot"]

SQUOTE_RE = re.compile("[^\\'\n]*$")
def quote(s):
    if SQUOTE_RE.match(s):
        return "'" + s + "'"
    raise NotImplementedError("Don't know how to escape %r" % s)

def mkStrSetter(prop):
    def setter(self, val):
        self("set %s %s" % (prop, quote(val)))
    return property(fset = setter)

def mkGenSetter(prop):
    def setter(self, val):
        self("set %s %s" % (prop, val))
    return property(fset = setter)

class Gnuplot(object):
    __slots__ = ["__g", "__cmds", "__plot"]

    def __init__(self):
        self.__g = subprocess.Popen(["gnuplot", "--persist"],
                                    stdin = subprocess.PIPE)
        self.__cmds = []
        self.__plot = []

    def __call__(self, cmd):
        self.__cmds.append(cmd)

    def __flush(self):
        for cmd in self.__cmds:
            print cmd
            # Unfortunately, there's no way to check if the command
            # succeeded, short of running gnuplot in a PTY.
            print >> self.__g.stdin, cmd
        self.__cmds[:] = []

    xlabel = mkStrSetter("xlabel")
    ylabel = mkStrSetter("ylabel")
    ytics = mkGenSetter("ytics")
    yrange = mkGenSetter("yrange")
    y2label = mkStrSetter("y2label")
    y2tics = mkGenSetter("y2tics")
    title = mkStrSetter("title")

    def addData(self, data, axis = None, title = None, with_ = None,
                linecolor = None, pointtype = None):
        # We intentionally leave these temp files around because
        # Gnuplot re-reads them whenever we zoom the graph.
        fd, path = tempfile.mkstemp(prefix = "gnuplot")
        f = os.fdopen(fd, "w")
        for tup in data:
            print >> f, "\t".join(map(str, tup))
        cmd = "%r" % path

        if with_ == None and (linecolor != None or pointtype != None):
            with_ = "points"
        if axis != None:
            cmd += " axis " + axis
        if title != None:
            cmd += " title " + quote(title)
        if with_ != None:
            cmd += " with " + with_
        if linecolor != None:
            cmd += " linecolor %d" % linecolor
        if pointtype != None:
            cmd += " pointtype %d" % pointtype
        self.__plot.append(cmd)
        return self

    def plot(self):
        if not len(self.__plot):
            raise ValueError("Nothing to plot")
        self("plot " + ", ".join(self.__plot))
        self.__flush()
        self.__plot[:] = []
