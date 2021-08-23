import matplotlib
matplotlib.use('Agg')



import seaborn as sns
import pandas as pd


def makePlotFromFile(filename):
    data_list = []
    with open(filename) as file_handle:
        lines = file_handle.read().split('\n')
        for line in lines:
            if(len(line.split(',')) == 4):
                mode = int(line.split(',')[0])
                if(mode == 1):
                    mode = 'Normal'
                if(mode == 2):
                    mode = 'Pack'
                if(mode == 3):
                    mode = 'Derived'
                gridSize = int(line.split(',')[2])
                if(gridSize == 16*16):
                    gridSize = "1:16"
                if(gridSize == 32*32):
                    gridSize = "2:32"
                if(gridSize == 64*64):
                    gridSize = "3:64"
                if(gridSize == 128*128):
                    gridSize = "4:128"
                if(gridSize == 256*256):
                    gridSize = "5:256"
                if(gridSize == 512*512):
                    gridSize = "6:512"
                if(gridSize == 1024*1024):
                    gridSize = "7:1024"
                time = float(line.split(',')[3])
                data_list.append([mode, gridSize, time])

    
    df = pd.DataFrame(data_list,
                       columns=['Mode', 'N', 'Time']
                      )
    df['Mode'] = df['Mode'].astype('string') 
    df['N'] = df['N'].astype('string')  
    
    
    sns.set(rc={'figure.figsize':(11.7,8.27)})
    ax = sns.lineplot(x="N", y="Time", hue="Mode", data=df, palette="Set1", ci = None)
    ax = sns.boxplot(x="N", y="Time", hue="Mode", data=df, palette="Set1")
    ax.grid(which='major', linewidth=1.0)
    return ax
    
for P in [16, 36, 49, 64]:
    ax = makePlotFromFile('./data_'+str(P))
    fig = ax.get_figure()
    fig.savefig("plot"+str(P)+".png")
    ax.clear()
