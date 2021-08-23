import random
import subprocess
from subprocess import call
import os
import timeit


#Your statements here


try:
    os.remove("data")
except OSError:
    pass

diff_conf4 = [[4,1],[2,2],[1,4]]
diff_conf16 = [[4,4],[2,8]]                

bashCommand = "make"
process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
output, error = process.communicate()

bashCommand = "chmod 700 gen_host_file.sh"
process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
output, error = process.communicate()

bashCommand = "chmod 700 run_exec.sh"
process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
output, error = process.communicate()


for exe in range(0,10):
    print(exe)
    start = timeit.default_timer()
    for P in [4,16]:
        for ppn in [1,8]:
            sppn = str(ppn)
            if P==4:
                ind = random.randint(0,2)
                groups = str(diff_conf4[ind][0])
                nodes_groups = str(diff_conf4[ind][1])
                rc = subprocess.check_call("./gen_host_file.sh %s %s %s" % (groups, nodes_groups, sppn), shell=True)
            elif P==16:
                ind = random.randint(0,1)
                groups = str(diff_conf16[ind][0])
                nodes_groups = str(diff_conf16[ind][1])
                rc = subprocess.check_call("./gen_host_file.sh %s %s %s" % (groups, nodes_groups, sppn), shell=True)
            for D in [16,256,2048]:
                newD = str((D*1024))
                np = str((P*ppn))
                rc = subprocess.check_call("./run_exec.sh %s %s %s" % (np, newD, sppn), shell=True)    
    stop = timeit.default_timer()
    print('-------------------------------------Time: '+str((stop - start)))  

