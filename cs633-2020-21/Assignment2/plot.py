import pandas as pd
import seaborn as sns
import numpy as np
import matplotlib.pyplot as plt


file1 = open('data', 'r')
Lines = file1.readlines()
 
times = [[],[],[],[]]
index = 0
count = 1
for line in Lines:
    times[index].append(float(line.strip()))
    if(count%2 == 0):
        index = (index + 1)%4
    count += 1

sns.set()


# In[48]:


bcast_inp = pd.DataFrame.from_dict({
    "D": [],
    "P": [],
    "ppn": [],
    "mode": [],  # 1 --> optimized, 0 --> standard
    "time": [],
})

reduce_inp = pd.DataFrame.from_dict({
    "D": [],
    "P": [],
    "ppn": [],
    "mode": [],  # 1 --> optimized, 0 --> standard
    "time": [],
})

gather_inp = pd.DataFrame.from_dict({
    "D": [],
    "P": [],
    "ppn": [],
    "mode": [],  # 1 --> optimized, 0 --> standard
    "time": [],
})

alltoall_inp = pd.DataFrame.from_dict({
    "D": [],
    "P": [],
    "ppn": [],
    "mode": [],  # 1 --> optimized, 0 --> standard
    "time": [],
})


# In[49]:

index = 0
for execution in range(10):
    for P in [4, 16]:
        for ppn in [1, 8]:
            for D in [16, 256, 2048]:
                # bcast
                bcast_inp = bcast_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 0, "time": times[0][index]
                }, ignore_index=True)
                bcast_inp = bcast_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 1, "time": times[0][index+1]
                }, ignore_index=True)
                # reduce
                reduce_inp = reduce_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 0, "time": times[1][index]
                }, ignore_index=True)
                reduce_inp = reduce_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 1, "time": times[1][index+1]
                }, ignore_index=True)
                # gather
                gather_inp = gather_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 0, "time": times[2][index]
                }, ignore_index=True)
                gather_inp = gather_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 1, "time": times[2][index+1]
                }, ignore_index=True)
                #alltoall
                alltoall_inp = alltoall_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 0, "time": times[3][index]
                }, ignore_index=True)
                alltoall_inp = alltoall_inp.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 1, "time": times[3][index+1]
                }, ignore_index=True)
                

                index += 2

bcast_inp["(P, ppn)"] = list(map(lambda x, y: ("(" + x + ", " + y + ")"), map(str, bcast_inp["P"]), map(str, bcast_inp["ppn"])))
reduce_inp["(P, ppn)"] = list(map(lambda x, y: ("(" + x + ", " + y + ")"), map(str, reduce_inp["P"]), map(str, reduce_inp["ppn"])))
gather_inp["(P, ppn)"] = list(map(lambda x, y: ("(" + x + ", " + y + ")"), map(str, gather_inp["P"]), map(str, gather_inp["ppn"])))
alltoall_inp["(P, ppn)"] = list(map(lambda x, y: ("(" + x + ", " + y + ")"), map(str, alltoall_inp["P"]), map(str, alltoall_inp["ppn"])))

# In[50]:


sns.catplot(x="(P, ppn)", y="time", data=bcast_inp, kind="box", col="D", hue="mode")
plt.savefig('plot_Bcast.jpg')
# bcast_inp.to_csv("bcast.csv")

sns.catplot(x="(P, ppn)", y="time", data=reduce_inp, kind="box", col="D", hue="mode")
plt.savefig('plot_Reduce.jpg')
# reduce_inp.to_csv("reduce.csv")


sns.catplot(x="(P, ppn)", y="time", data=gather_inp, kind="box", col="D", hue="mode")
plt.savefig('plot_Gather.jpg')
# gather_inp.to_csv("gather.csv")

sns.catplot(x="(P, ppn)", y="time", data=alltoall_inp, kind="box", col="D", hue="mode")
plt.savefig('plot_Alltoallv.jpg')
# alltoall_inp.to_csv("alltoallv.csv")

# In[ ]:




