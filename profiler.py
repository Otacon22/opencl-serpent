#!/usr/bin/python
import os
from time import time

fd = open("performance_data"+str(int(time()))+".csv","w")

for workitems in [32,64,128,256,512,1024,2048,4096,8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576]:
    for loops in [1,2,4,8,16,32,64,128,256,512,1024,2048,4096,10000,15000,20000]:
	for wg-size in [1,2,4,8,16,32,64,128,256,512,1024]:
	    print "Executing: "+str(workitems)+","+str(loops)
            cmd = "./main -c -w "+str(workitems)+" -b "+str(loops)+" -g "+str(wg-size)
            out = os.popen(cmd).read().split("\n")[1]
            fd.write(out+"|"+cmd+"\n")
            fd.flush()
