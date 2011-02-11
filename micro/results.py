import pickle
import errno
import copy
import sys

class ResultTable:
    def __init__(self, name, columns):
        self.name = name
        self.columns = copy.copy(columns)
        self.rows = []

    def add_row(self, vals):
        self.rows.append(copy.copy(vals))

    def gnuplot(self, columnHeader = False):
        s = ''
        if columnHeader:
            s += '#%-7s' % self.columns[0]
            for col in self.columns[1:]:
                s += ' %16s' % col
            s += '\n'
                
        for row in self.rows:
            s += '%-8s' % str(row[0])
            for val in row[1:]:
                s += ' %16s' % (str(val))
            s += '\n'

        return s

    def sort(self, fn, reverse):
        self.rows = sorted(self.rows,
                           key=fn,
                           reverse=reverse)

class Results:
    def __init__(self, name):
        self.name = name
        self.table = {}

    def add_table(self, table):
        if table in self.table:
            raise Exception('Results:add_table exits')
        self.table[table.name] = table

    def get_table(self, name):
        if name in self.table:
            return self.table[name]
        return None

def save_results(fileName, results):
    output = open(fileName, 'wb')
    pickle.dump(results, output)
    output.close()

def open_results(fileName):
    results = None
    try:
        pickleFile = open(fileName, 'r')
        results = pickle.load(pickleFile)
        pickleFile.close()
    except IOError, e:
        if e.errno != errno.ENOENT:
            raise
        results = {}
    return results

def main(argv=None):
    allResults = open_results('foo.res')

    table = ResultTable('max-throughput', ['version', 'cores', 'throughput'])
    result = Results('memclone')
    result.add_table(table)
    allResults['memclone'] = result
    
    for i in range(1, 10):
        table.add_row([10 - i, i, 4])

    print allResults['memclone'].get_table('max-throughput').gnuplot(columnHeader = True)
    allResults['memclone'].get_table('max-throughput').sort(lambda x: x[0], False)
    print allResults['memclone'].get_table('max-throughput').gnuplot(columnHeader = True)

#    print allResults

    save_results('foo.res', allResults)

    

if __name__ == '__main__':
    sys.exit(main())
