import pandas as pd
import seaborn as sns
import numpy as np
import matplotlib.pyplot as plt


file1 = open('data', 'r')
Lines = file1.readlines()
 
time_data = []
index = 0
count = 1
for line in Lines:
    if(count%3 == 0):
        time_data.append(float(line))
    count += 1

sns.set()


# In[48]:


graph_inp = pd.DataFrame.from_dict({
    "node": [],
    "ppn": [],
    "time": [],
})


# In[49]:

index = 0
for execution in range(5):
    for node in [1, 2]:
        for ppn in [1, 2, 4]:
            # bcast
            graph_inp = graph_inp.append({
                "node": int(node), "ppn": int(ppn), "time": time_data[index]
            }, ignore_index=True)
            index += 1

graph_inp["(node, ppn)"] = list(map(lambda x, y: ("(" + str(x) + ", " + str(y) + ")"), map(int, graph_inp["node"]), map(int, graph_inp["ppn"])))

# In[50]:

sns.catplot(x="(node, ppn)", y="time", data=graph_inp, kind="box", )
plt.savefig('plot.jpg')
# bcast_inp.to_csv("bcast.csv")

# In[ ]:




