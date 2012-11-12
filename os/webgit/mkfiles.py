import sys
import os
import random
from optparse import OptionParser
import string

def gen_filename(previous_files):
    ''' Generate a new (8 char) file name, randomly. Be sure not to use a name
    that's already been generated.
    '''
    choices = string.hexdigits[:16]
    while True:
        fname = ''
        for i in range(8):
            fname += random.choice(choices)
        if fname not in previous_files:
            return fname + ".html"
    
def generate_file(fname):
    fsize = int(1000 * random.paretovariate(1.2))
    outf = open(fname, "w")
    outf.write('A'*fsize)
    outf.close()

def main(numfiles, thedir, filelist):
    '''
    Create numfiles files on disk in some sort of hierarchical arrangement,
    starting in the current directory. Spit out a list of all files generated
    in the file "filelist". The distribution of file sizes is pareto.
    
    This program should be run before running the webdriver.py program to
    generate load on your webserver.
    '''
    try:
        os.mkdir(thedir)
    except OSError,e:
        if 'file exists' not in str(e).lower():
            raise e

    outlist = open(filelist, "w")
    files_generated = set()
    
    for i in range(numfiles):
        fname = gen_filename(files_generated)
        fullpath = thedir + '/' + fname
        files_generated.add(fullpath)
        generate_file(fullpath)
        print >>outlist, "{0}".format(fullpath)

    outlist.close()

parser = OptionParser()
parser.add_option("-f", "--filelist", dest="filelist",
                default="filelist.txt",
                help="write list of files generated to FILE (default: filelist.txt)", 
                metavar="FILE")
parser.add_option("-d", "--directory", dest="thedir",
                default="testfiles",
                help="name of the directory in which to write the test files")
parser.add_option("-n", "--numfiles", dest="numfiles",
                help="generated a specified number of files (default=1)",
                default=1, type="int")
(options, args) = parser.parse_args()
        
main(options.numfiles, options.thedir, options.filelist)
