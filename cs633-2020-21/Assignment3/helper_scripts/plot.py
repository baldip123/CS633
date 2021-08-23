#!/usr/bin/env python
# coding: utf-8

# In[47]:


import pandas as pd
import seaborn as sns
import numpy as np
import matplotlib.pyplot as plt

sns.set()


# In[48]:


demo_input_format = pd.DataFrame.from_dict({
    "D": [],
    "P": [],
    "ppn": [],
    "mode": [],  # 1 --> optimized, 0 --> standard
    "time": [],
})


# In[49]:


for execution in range(10):
    for P in [4, 16]:
        for ppn in [1, 8]:
            for D in [16, 256, 2048]:
                # Change with the actual data
                demo_input_format = demo_input_format.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 1, "time": np.random.rand() / 10
                }, ignore_index=True)
                demo_input_format = demo_input_format.append({
                    "D": D, "P": P, "ppn": ppn, "mode": 0, "time": np.random.rand()
                }, ignore_index=True)

demo_input_format["(P, ppn)"] = list(map(lambda x, y: ("(" + x + ", " + y + ")"), map(str, demo_input_format["P"]), map(str, demo_input_format["ppn"])))

print(demo_input_format)

# In[50]:


sns.catplot(x="(P, ppn)", y="time", data=demo_input_format, kind="box", col="D", hue="mode")
plt.show()

# In[ ]:




