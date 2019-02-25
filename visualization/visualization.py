"""
Must Pip install:
    pandas
    seaborn
    matplotlib
"""
import sys
import pandas as pd
from matplotlib import pyplot as plt
import seaborn as sns
import os
import shutil
"""
Creates SwarmPlot for desired Input Queue
Then moves plot images into "Plots" subdirectory
"""
def SwarmSubPlot( fd, input, directory):
    figure = sns.swarmplot(x="Output" , y= "Time",hue ="Algorithm", dodge = True, data= fd[fd['Input']==input] )
    title = 'Plot' + str(input)      
    figure.set_title(title)
    fig = figure.get_figure()
    filename = 'Queue'+str(input)+".png"
    fig.savefig(filename)
    CWD = os.getcwd()
    shutil.move(os.path.join(CWD,filename), directory)
"""
Checks to see if "Plots" Folder exists, creates one if it doesnt
"""
def checkDirectory():
    foldername = 'Plots'
    CWD = os.getcwd()
    final_directory = os.path.join(CWD, foldername) 
    if not os.path.exists(final_directory):
        os.makedirs(final_directory)
        print("Folder Created:", foldername)
        
       
checkDirectory()
fd = pd.read_csv('dataset.csv')
#fd = pd.read_csv(sys.argv[1])
for i in range(1, 9):
    print("I value:",i)
    try:
        SwarmSubPlot(fd, i, os.path.join(os.getcwd(), 'Plots'))
    except Exception:
        pass


