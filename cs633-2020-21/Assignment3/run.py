import subprocess
from subprocess import call
import sys
import os

#change the file name if needed
file_name = "tdata.csv"

try:
    os.remove("data")
except OSError:
    pass

outf = "data"

# executing the make isntruciton
bashCommand = "make"
process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
output, error = process.communicate()

# giving permissions to the bash files
bashCommand = "chmod 700 gen_host_file.sh"
process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
output, error = process.communicate()

# the main bash file that runs the code
bashCommand = "chmod 700 run_exec.sh"
process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
output, error = process.communicate()


for i in range(10):
    for node in [1,2]:
        for ppn in [1,2,4]:
            # print(str(i)+","+str(node)+","+str(ppn))
            rc = subprocess.check_call("./gen_host_file.sh %s %s %s" % ("1", str(node), str(ppn)), shell=True)    
            rc = subprocess.check_call("./run_exec.sh %s %s %s" % (str((node*ppn)),file_name, outf), shell=True)    


bashCommand = "python3 plot.py"
process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE)
output, error = process.communicate()

# FpQv8kTe9snqRE9