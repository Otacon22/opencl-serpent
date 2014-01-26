#!/usr/bin/python
import os
from time import timep

fd = open("performance_data"+str(int(time()))+".csv","w")

for workitems in [1,16,32,64,128,256,512,1024,2048,4096]:
    for loops in [1,16,32,64,128,256,512,1024,2048,4096,10000]:
        print "Executing: "+str(workitems)+","+str(loops)
        out = os.popen("./main -c -w "+str(workitems)+" -b "+str(loops)).read().split("\n")[1]
        fd.write(out+"\n")
        fd.flush()
