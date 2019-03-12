"""
Must Pip install:
    pandas
    seaborn
    matplotlib
    pathlib
"""
import matplotlib
matplotlib.use('Agg')
import sys
import pandas as pd
from matplotlib import pyplot as plt
import seaborn as sns
import os
import shutil
from pathlib import Path
from mp1_toolkits.axes_grid1 import ImageGrid
import numpy as np
NUMPLOT =0
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

"""
Creates SwarmPlot for desired Input Queue
Then moves plot images into "Plots" subdirectory
Takes in:
    fd: panda database
    input: Input Queue for graph
    directory: folder to move content into
"""
def SwarmSubPlot( fd, input, baseName, directory):
    global NUMPLOT
    plt.figure(NUMPLOT)
    sns.swarmplot(x="Output" , y= "Packet",hue ="Algorithm", dodge = True, data= fd[fd['Input']==input] )
    title = 'Plot ' + baseName +' Queue' + str(input)      
    plt.figure(NUMPLOT).set_title(title)
   # fig = figure.get_figure()
    filename = 'swarm_'+baseName+str(input)+".png"
    plt.figure(NUMPLOT).savefig(filename)
    CWD = os.getcwd()
    shutil.copy(os.path.join(CWD,filename), directory)
    os.remove(os.path.join(CWD,filename))

def scatterSubPlot( fd, input, baseName, directory):
	plt.figure
	scatt = sns.scatterplot(x="Input" , y= "Packet",hue ="Output", dodge = True, data= fd)
    scatt.fig.suptitle(baseName)
	plt.plot(data = fd)
    filename = 'scatter_'+ baseName+".png"
    scatt.savefig(filename)
	CWD = os.getcwd()
    shutil.copy(os.path.join(CWD,filename), directory)
    os.remove(os.path.join(CWD,filename))
"""
Creates CatPlot for desired Algorithm
Then moves plot images into "Plots" subdirectory
Takes in:
    fd: panda database
    input: Input Queue for graph
    directory: folder to move content into
"""
def catSubPlot( fd, fileNum, baseName, directory):
    plt.figure
    cat = sns.catplot(x="Input" , y= "Packet",hue ="Output", dodge = True, data= fd)
    cat.fig.suptitle(baseName)
    filename = 'cat_'+ baseName+".png"
    cat.savefig(filename)
    CWD = os.getcwd()
    shutil.copy(os.path.join(CWD,filename), directory)
    os.remove(os.path.join(CWD,filename))

"""
Creates Bar Graph for desired Input Queue
Then moves plot images into "Plots" subdirectory
Takes in:
    fd: panda database
    input: Input Queue for graph
	 
"""

def barSubPlot( fd, input, fileCount,directory):
    figNum = input+ fileCount
    #print("Fignum is:", figNum)
    plt.figure(figNum)
	
    title = 'InputQueue' + str(input)  
    outBar = sns.barplot(x="Output" , y= "Packet",hue ="Algorithm", dodge = True, data= fd[fd['Input']==input]).set_title(title)
    filename = 'bar_collection_'+str(input)+".png"
	
    plt.figure(figNum).savefig(filename)
    CWD = os.getcwd()
    shutil.copy(os.path.join(CWD,filename), directory)
    os.remove(os.path.join(CWD,filename))
    



"""
Runs through and creates Swarm plots for CSV file inputted
"""
def runSwarm(fileName):

    for i in range(1, 9):
        print("I value:",i)
        try:
            baseName =  Path(fileName).stem
            fd = pd.read_csv(fileName)
            SwarmSubPlot(fd, i, baseName, os.path.join(os.getcwd(), 'Plots'))
            del fd
        except Exception:
            pass
    
"""
Runs through and creates Swarm plots for CSV file inputted
"""
def runCat(fileName, fileNum):
    baseName =  Path(fileName).stem
    fd = pd.read_csv(fileName)
    catSubPlot(fd,fileNum, baseName, os.path.join(os.getcwd(), 'Plots'))
    del fd
         


"""
Finds all .csv files from specified directory and creates a cat Plot for that run
"""
def singleRun(directoryPath):
    numberFiles =0
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                print(os.path.splitext(filePath)[0])
                runCat(filePath, numberFiles)
				
                numberFiles +=1
    return numberFiles
    
"""
Reads in all csv files into database then runs
"""
def collectionRun(directoryPath, fileCount):
    list = []
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                print(file)
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                print(filePath)
                df = pd.read_csv(filePath)
                list.append(df)

    big_frame = pd.concat(list, axis = 0, ignore_index = True)
    image = nparrange(100).reshape((10,10))
	
    for i in range(1, 9):
        outBar[i]= barSubPlot(big_frame, i,fileCount, os.path.join(os.getcwd(), 'Plots'))
		outBar[i].imshow(image)
		
		""" 
		might need to change to a dot graph
		"""


directory = os.path.join(os.getcwd(),sys.argv[1])          
checkDirectory()
num =singleRun(directory)
collectionRun(directory, num)
print(num