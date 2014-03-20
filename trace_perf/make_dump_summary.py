#!/opt/local/bin/python

from os import listdir, makedirs
from os.path import isfile, join, exists
import numpy

# http://countergram.com/python-group-iterator-list-function
def group_iter(iterator, n, strict=False):
    """ Transforms a sequence of values into a sequence of n-tuples.
    e.g. [1, 2, 3, 4, ...] => [(1, 2), (3, 4), ...] (when n == 2)
    If strict, then it will raise ValueError if there is a group of fewer
    than n items at the end of the sequence. """
    accumulator = []
    for item in iterator:
        accumulator.append(item)
        if len(accumulator) == n: # tested as fast as separate counter
            yield tuple(accumulator)
            accumulator = [] # tested faster than accumulator[:] = []
            # and tested as fast as re-using one list object
    if len(accumulator) != 0:
        if strict:
            raise ValueError("Leftover values")
        yield tuple(accumulator)

def read_dump_file(dumpfile):
    lines = [line.strip() for line in open(dumpfile, 'r')]
    db = {}
    for entry in group_iter(lines, 3):
        db[entry[0]] = float(entry[1])

    return db

def get_dump_files():
    path = 'dump'
    files = [ f for f in listdir(path) if isfile(join(path,f)) ]
    dbs = {}
    
    for f in files:
        basename = f[0:f.find('.db')];
        db = read_dump_file(join(path,f))
        if not basename in dbs:
            dbs[basename] = {}

        dblist = dbs[basename]

        for text in db:
            if not text in dblist:
                dblist[text] = []
            dblist[text] += [db[text]]
        dbs[basename] = dblist

    return dbs

def make_dump_summary(dbs):
    sumdbs = {}
    for basename in dbs:
        db = dbs[basename]
        sumdb = {}
        for text in db:
            v = db[text]
            sumdb[text] = "min: %g, mean: %g, std: %g, max: %g, N: %d" % (numpy.min(v), numpy.mean(v), numpy.std(v), numpy.max(v), len(v))

        sumdbs[basename] = sumdb

    return sumdbs

def write_dump_summary(sumdbs):
    path = 'summary'
    if not exists(path):
        makedirs(path)

    for basename in sumdbs:
        filename = join(path,basename + ".txt")
        with open(filename, 'w') as f:
            db = sumdbs[basename]
            for text in db:
                infotext = db[text]
                f.write('%s\n' % text)
                f.write('%s\n' % infotext)
                f.write('\n')

dbs = get_dump_files()
sumdbs = make_dump_summary(dbs)
write_dump_summary(sumdbs)
