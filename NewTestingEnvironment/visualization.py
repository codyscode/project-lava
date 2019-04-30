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
Creates linePlot for desired Algorithm
OverLays two plots on top of each other:
    Dot-plot
    Line-graph
Then moves plot images into "Plots" subdirectory
Takes in:
    fd: panda database
    fileNum: keeps track of number of unique figures created
    basename: CSV file name to label title
    directory: folder to move content into
"""
def lineSubPlot( fd, fileNum, baseName, directory):
    fig = plt.figure(fileNum, figsize=(8,8))
    markerArray = ['1', "x", "*","o","d","^","s","8"]
    markColors = ['blue', 'orange', 'green','red','purple','brown','pink','grey']
    sns.set( rc={"lines.linewidth": 0.7})
    cat = sns.pointplot(x="Input" , y= "Packet",hue ="Output",
    ci = None, dodge = False, errwidth=None, data= fd,
    markers="", scale = 1)
    
    cat = sns.pointplot(x="Input" , y= "Packet",hue ="Output",
    ci = None, dodge = False, linestyles = " ", errwidth=None, data= fd,
    markers=markerArray, scale = 1.8).set_title(baseName)

    plt.ticklabel_format(style='plain', axis='y')
    
    symbol=[]
    for i in range (0,8):
        symbol.append( matplotlib.lines.Line2D([], [], color=markColors[i], marker=markerArray[i], linestyle='None', markersize=6, label=str(i+1)))

    lgd = plt.legend(title= "Output Queue Count" , loc='center left', bbox_to_anchor=(1, 0.5),
    handles=[symbol[0], symbol[1], symbol[2],symbol[3],symbol[4],symbol[5],symbol[6],symbol[7]])

    plt.xlabel("Input Queue Count")
    plt.ylabel("Packets Per Second")    
    filename = 'point_'+ baseName+".png"
    fig.savefig(filename, bbox_extra_artists=(lgd,), bbox_inches='tight')
    matplotlib.pyplot.close(fig)
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
Runs through and creates Swarm plots for CSV file inputted

"""
def runLine(fileName, fileNum):
    baseName =  Path(fileName).stem
    fd = pd.read_csv(fileName)
    lineSubPlot(fd,fileNum, baseName, os.path.join(os.getcwd(), 'Plots'))
    del fd
         


"""
Finds all .csv files from specified directory and creates a Line/Dot Plot for that individual run
"""
def singleRun(directoryPath):
    numberFiles =0
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                print(os.path.splitext(filePath)[0])
                runLine(filePath, numberFiles)
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
