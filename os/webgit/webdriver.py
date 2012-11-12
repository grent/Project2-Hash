from optparse import OptionParser
import urllib2
import random
import threading
import time

def makereq(num, url):
    '''
    Thread entry point: make an HTTP request
    '''
    begin = time.time()
    fh = urllib2.urlopen(url)
    garbage = fh.read()
    fh.close()
    end = time.time()
    print "Request {0} took {1:.3f} seconds".format(num, end-begin)


def main(filelist, numreqs, host, port):
    '''
    Generate load on a webserver, making requests to files that have names
    defined in the file "filelist". The series of requests should be generated
    in an exponentially distributed fashion to mimic real users.
    
    This program requires the "filelist" file that's generated from the
    mkfiles.py program. It can be run from any host (or even the same host as
    the webserver), as long as the filelist is present.
    '''
    infile = None
    try:
        infile = open(filelist)
    except Exception,e:
        print "Couldn't open file list in {0}: ".format(filelist),str(e)
        return
    filelist = infile.read().strip().split()
    infile.close()
        
    baseurl = "http://" + host + ":" + str(port) + "/"

    rnum = 0
    for req in range(numreqs):
        reqfile = random.choice(filelist)
        url = baseurl + reqfile
        t = threading.Thread(target=makereq, args=(rnum,url))
        t.run()

        # sleep for exponentially distributed amt of time,
        # avg of 1/2 second
        time.sleep(random.expovariate(1.0/0.5))
        rnum += 1


parser = OptionParser()
parser.add_option("-f", "--filelist", dest="filelist",
                default="filelist.txt",
                help="load list of files from FILE (default: filelist.txt)", 
                metavar="FILE")
parser.add_option("-r", "--numreqs", dest="numreqs",
                help="Number of HTTP requests to make",
                default=1, type="int")
parser.add_option("-p", "--port", dest="port",
                help="HTTP server port (default: 3000)",
                default=3000, type="int")
parser.add_option("-t", "--host", dest="host",
                help="HTTP server host (default: localhost)",
                default="127.0.0.1", type="str")
(options, args) = parser.parse_args()
        
main(options.filelist, options.numreqs, options.host, options.port)
