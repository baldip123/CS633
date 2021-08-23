import numpy as np
import pandas as pd
import sys

row = int(sys.argv[1]) + 1
col = int(sys.argv[2]) + 1

# df = pd.DataFrame(np.random.randint(0,100,size=(100, 4)), columns=list('ABCD'))
df = pd.DataFrame(np.random.normal(0,40,size=(row, col)))
df.to_csv("temp.csv",header=False)