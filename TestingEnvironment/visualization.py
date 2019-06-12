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
import time

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
Creates CatPlot for desired Algorithm
Then moves plot images into "Plots" subdirectory
Takes in:
    fd: panda database
    basename: Gets Name of CSV file to Set as name of image
    directory: folder to move content into
"""
def catSubPlot( fd, baseName, directory):
    plt.figure
    cat = sns.catplot(x="Input" , y= "Bits",hue ="Output", dodge = True, data= fd)
    cat.set(xlabel="Input Queue Count", ylabel="Gigabits Per Second")
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
    sns.set_style("whitegrid")
    plt.ticklabel_format(style='plain', axis='y')
    figNum = input+ fileCount
    plt.figure(figNum, figsize=(10, 10))
    title = 'Input Queue Count: ' + str(input)  
    sns.barplot(x="Output" , y= "Bits",hue ="Algorithm", dodge = True, data= fd[fd['Input']==input]).set_title(title)
    plt.xlabel("Output Thread Count")
    plt.ylabel("Gigabits Per Second(Gbs)")
    filename = 'bar_collection_'+str(input)+".png"
    plt.figure(figNum).savefig(filename)
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
def posterPlot( big_frame, directory, fileCount):
    sns.set_style("whitegrid")
    posterColors =  ["#db4437", "#4285f4", "#0f9d58"]
    plt.ticklabel_format(style='plain', axis='y')
    qData = big_frame.query('((Input == "8") and (Output == "8")) or ((Input =="4") and (Output =="8")) or ((Input =="8") and (Output =="4"))').copy()
    qData.loc[qData.Input < qData.Output, 'Condition'] = 0
    qData.loc[qData.Input == qData.Output, 'Condition'] = 1
    qData.loc[qData.Input > qData.Output, 'Condition'] = -1
    figNum = fileCount

    newDB = qData.copy()
    newDB['Bits'] = newDB['Bits']/(1000000000) #changes units from bits to Gigabits
    
    plt.figure(figNum, figsize=(10, 10))
    title = 'Performance in Certain Test Cases'  
    plot = sns.barplot(x="Algorithm" , y= "Bits",hue ="Condition", dodge = True, data= newDB, palette = posterColors).set_title(title)
    plot.axes.set_xticklabels(["1","2","3","4","5","6","7","8"])
    plt.xlabel("Algorithms")
    plt.ylabel("Gigabits Per Second (Gbs)")

    lgd = plt.legend(title= "Performance Type" , loc='center left', bbox_to_anchor=(1, 0.5), labels=["8x4", "4x8","8x8"])
    for i in range(0,3):
        lgd.legendHandles[i].set_color(posterColors[i])
    
    filename = "Poster"+".png"
    plt.figure(figNum).savefig(filename, bbox_extra_artists=(lgd,), bbox_inches='tight')
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
    newDB = fd.copy()
    sns.set_style('darkgrid')
    newDB['Bits'] = newDB['Bits']/(1000000000) #changes units from bits to Gigabits
    #sns.set(font_scale =2)
    fig = plt.figure(fileNum, figsize=(10,10))
    markerArray = ['1', "x", "*","o","d","^","s","8"]
    markColors = ['blue', 'orange', 'green','red','purple','brown','pink','grey']
    plt.ticklabel_format(style='plain', axis='y')

    linePlotLabel = []
    for i in range(0,8):
        if i == 0:
            linePlotLabel.append(str(i+1) + " Output Thread" )
        else:
            linePlotLabel.append(str(i+1) + " Output Threads" ) 


    sns.set( rc={"lines.linewidth": 0.7})
    cat = sns.pointplot(x="Input" , y= "Bits",hue ="Output", ci = None, dodge = False, errwidth=None, palette= markColors, data= newDB, markers="", scale = 1)
    cat = sns.pointplot(x="Input" , y= "Bits",hue ="Output",ci = None, dodge = False, linestyles = " ", errwidth=None, palette= markColors, data= newDB, markers=markerArray, scale = 2.1).set_title(baseName)
    
    symbol=[]
    for i in range (0,8):
        symbol.append( matplotlib.lines.Line2D([], [], color=markColors[i], marker=markerArray[i], linestyle='None', markersize=6, label=str(i+1)))

    lgd = plt.legend(title= "Legend" , loc='center left', bbox_to_anchor=(1, 0.5), handles=symbol, labels = linePlotLabel)

    plt.xlabel("Input Thread Count")
    plt.ylabel("Gigabits Per Second (Gb/s)")    
    filename = 'point_'+ baseName+".png"
    fig.savefig(filename, bbox_extra_artists=(lgd,), bbox_inches='tight')
    matplotlib.pyplot.close(fig)
    CWD = os.getcwd()
    shutil.copy(os.path.join(CWD,filename), directory)
    os.remove(os.path.join(CWD,filename))


"""
Runs through and creates CAT plots for CSV file inputted
"""
def runCat(fileName):
    baseName =  Path(fileName).stem
    fd = pd.read_csv(fileName)
    catSubPlot(fd, baseName, os.path.join(os.getcwd(), 'Plots'))
    del fd


"""
Runs through and creates Line-plots for CSV file inputted

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
                df = pd.read_csv(filePath)
                csvlist.append(df)

    big_frame = pd.concat(csvlist, axis = 0, ignore_index = True)
    big_frame = big_frame.sort_values(by=['Algorithm'])
    image = np.arange(100).reshape((10,10))
    for i in range(1, 9):
        barSubPlot(big_frame, i,fileCount, os.path.join(os.getcwd(), 'Plots'))

"""
Develops a Plot of 2 differnt performance cases for all algorithms
    Performance Cases:
        8x8
        8x4
        4x8
"""
def posterRun(directoryPath, fileCount):
    csvlist = []
    posterColors =  ["#db4437", "#4285f4", "#0f9d58"]

    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                df = pd.read_csv(filePath)
                csvlist.append(df)

    big_frame = pd.concat(csvlist, axis = 0, ignore_index = True)
    big_frame = big_frame.sort_values(by=['Algorithm'])
    posterPlot(big_frame,directoryPath, fileCount)

    
"""
Creates a mean plot of all 64 iterations of each algorithms to develop a graph that easily shows average performance
"""    
def algorithmComparison(directoryPath, figNum):
    csvlist = []
    sns.set_style("whitegrid")
    
    #Walks through and finds all CSV files in given directory
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                df = pd.read_csv(filePath)
                csvlist.append(df)

    #Obtains all stored data and sorts it
    big_frame = pd.concat(csvlist, axis = 0, ignore_index = True)
    big_frame = big_frame.sort_values(by=['Algorithm'])
    newDB = big_frame.copy()
    newDB['Bits'] = newDB['Bits']/(1000000000) #changes units from bits to Gigabits

   
    
    plt.figure(figNum, figsize=(20, 20))
    sns.set(font_scale =2)
    title = 'Average Algorithm Performance '  
    sns.barplot(x="Algorithm" , y= "Bits",dodge = True, data= newDB, palette= "bright").set_title(title)
    plt.xlabel("")
    plt.ylabel("Gigabits Per Second(Gbs)")
    filename = 'Final_Comparison'+".png"

    plt.figure(figNum).savefig(filename)
    CWD = os.getcwd()
    shutil.copy(os.path.join(CWD,filename), directory)
    os.remove(os.path.join(CWD,filename))

"""
Efficiently Run's all the 3 functions together:
    singleRun
    CollectionRun
    PosterRun
"""
def efficientRun(directoryPath):
    plotFolder = os.path.join(os.getcwd(), 'Plots')
    fileCount = 0
    csvlist = []
    outBar = [8]
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                print(os.path.splitext(filePath)[0])
                runLine(filePath, fileCount)
                fileCount +=1
                df = pd.read_csv(filePath)
                csvlist.append(df)

    big_frame = pd.concat(csvlist, axis = 0, ignore_index = True)
    big_frame = big_frame.sort_values(by=['Algorithm'])
    image = np.arange(100).reshape((10,10))
    for i in range(1, 9):
        barSubPlot(big_frame, i,fileCount,plotFolder)

    posterPlot(big_frame,plotFolder, fileCount+9)

                

"""
Calls respective function calls,
efficientRun calls:
    singleRun
    CollectionRun
    PosterRun
Then algorithmComparison creates an mean graph of all 64 iterations of an algorithms
"""
directory = os.path.join(os.getcwd(),sys.argv[1])          
checkDirectory()
efficientRun(directory)
algorithmComparison(directory, 11)