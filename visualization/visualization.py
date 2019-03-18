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
from mpl_toolkits.axes_grid1 import ImageGrid
import numpy as np

"""
Checks to see if "Plots" Folder exists, creates one if it doesnt
    Plots Subdirectory will hold all images created
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
    basename: 
    directory: folder to move content into
"""
def SwarmSubPlot( fd, input, baseName, directory):
    sns.swarmplot(x="Output" , y= "Packet",hue ="Algorithm", dodge = True, data= fd[fd['Input']==input] )
    title = 'Plot ' + baseName +' Queue' + str(input)      
    plt.figure.set_title(title)
    filename = 'swarm_'+baseName+str(input)+".png"
    plt.figure.savefig(filename)
    CWD = os.getcwd()
    shutil.copy(os.path.join(CWD,filename), directory)
    os.remove(os.path.join(CWD,filename))


"""
Creates CatPlot for desired Algorithm
Then moves plot images into "Plots" subdirectory
Takes in:
    fd: panda database
    basename: Gets Name of CSV file to Set as name of image
    directory: folder to move content into
"""
def catSubPlot( fd, baseName, directory):
    plt.figure
    cat = sns.catplot(x="Input" , y= "Packet",hue ="Output", dodge = True, data= fd)
    cat.set(xlabel="Input Queue Count", ylabel="Packets Per Second")
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
    input: Desired Input Queue dataset
    FileCount: Number inputed to differentiate figures to prevent overlapping figures
    Directory: folder to move content into
"""
def barSubPlot( fd, input, fileCount,directory):
    figNum = input+ fileCount
    plt.figure(figNum, figsize=(10, 10))
    title = 'Input Queue Count: ' + str(input)  
    sns.barplot(x="Output" , y= "Packet",hue ="Algorithm", dodge = True, data= fd[fd['Input']==input]).set_title(title)
    plt.xlabel("Output Queue Count")
    plt.ylabel("Packets Per Second")
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
Runs through and creates CAT plots for CSV file inputted
"""
def runCat(fileName):
    baseName =  Path(fileName).stem
    fd = pd.read_csv(fileName)
    catSubPlot(fd, baseName, os.path.join(os.getcwd(), 'Plots'))
    del fd
         

"""
Finds all '.csv' files from specified directory and creates a cat Plot for that run
"""
def singleRun(directoryPath):
    numberFiles =0
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                print(os.path.splitext(filePath)[0])
                runCat(filePath)
                numberFiles +=1
    return numberFiles
    

"""
Reads in all csv files into database then 
Creates Bar Graphs for Each Data set of Queues 1-8
"""
def collectionRun(directoryPath, fileCount):
    csvlist = []
    outBar = [8]
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                print(file)
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                print(filePath)
                df = pd.read_csv(filePath)
                csvlist.append(df)

    big_frame = pd.concat(csvlist, axis = 0, ignore_index = True)
    image = np.arange(100).reshape((10,10))
    for i in range(1, 9):
        barSubPlot(big_frame, i,fileCount, os.path.join(os.getcwd(), 'Plots'))
   

"""
Obtains Full path of desired folder full of CSV file
Then we check to see Plots/ exists, if it doesn't, folder is made
Then CAT and BAR plots images of the data are put into sub-directory
"""
directory = os.path.join(os.getcwd(),sys.argv[1])          
checkDirectory()
num =singleRun(directory)
collectionRun(directory, num)
