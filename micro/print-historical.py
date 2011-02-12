#!/usr/bin/python
import pickle
import errno
import copy
import sys
import results

def usage(argv):
    print 'usage: %s pickle-file benchmark' % argv[0]
    exit(1)

def main(argv=None):
    if argv is None:
        argv = sys.argv
    if len(argv) < 3:
        usage(argv)

    pickleFile = argv[1]
    name = argv[2]

    allResults = results.open_results(pickleFile)
    benchResults = allResults[name]

    benchResults.get_table('max-throughput').sort(lambda x: x[0], False)
    benchResults.get_table('max-scale').sort(lambda x: x[0], False)

    print '# %s-max-throughput' % name
    print benchResults.get_table('max-throughput').gnuplot(True)
    print ''
    print '# %s-max-scale' % name
    print benchResults.get_table('max-scale').gnuplot(True)

if __name__ == '__main__':
    sys.exit(main())
