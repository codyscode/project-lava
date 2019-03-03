"""
Must Pip install:
    pandas
    seaborn
    matplotlib
    pathlib
"""
import sys
import pandas as pd
from matplotlib import pyplot as plt
import seaborn as sns
import os
import shutil
from pathlib import Path

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
    input: Input Queue for garph
"""
def SwarmSubPlot( fd, input, baseName, directory):
    figure = sns.swarmplot(x="Output" , y= "Packet",hue ="Algorithm", dodge = True, data= fd[fd['Input']==input] )
    title = 'Plot ' + baseName +' Queue' + str(input)      
    figure.set_title(title)
    fig = figure.get_figure()
    filename = baseName+str(input)+".png"
    fig.savefig(filename)
    CWD = os.getcwd()
    shutil.move(os.path.join(CWD,filename), directory)

"""
Runs through and creates Swarm plots for CSV file inputted

"""
def runSwarm(fileName):
    #print(fileName)
    baseName =  Path(fileName).stem
    fd = pd.read_csv(fileName)
    for i in range(1, 9):
        print("I value:",i)
        try:
            SwarmSubPlot(fd, i, baseName, os.path.join(os.getcwd(), 'Plots'))
        except Exception:
            pass
    



"""
Finds all .csv files from specified directory and creates Plots
"""
def main(directoryPath):
    for root,dirs,files in os.walk(directoryPath):
        for file in files:
            if file.endswith(".csv"):
                #print(file)
                folderPath = os.path.join(os.getcwd(),sys.argv[1])
                filePath = os.path.join(folderPath,file)
                print(os.path.splitext(filePath)[0])
                runSwarm(filePath)
                
"""

fd = pd.read_csv('mockrun.csv')
figure = sns.scatterplot(x="Input" , y= "Packet",hue ="Output", data= fd )
#title = 'Plot ' + baseName +' Queue' + str(input)      
#figure.set_title(title)
fig = figure.get_figure()
#filename = baseName+str(input)+".png"
fig.savefig('test.png')
#CWD = os.getcwd()
#shutil.move(os.path.join(CWD,filename), directory)
"""


#setupmean graph for final visualization

checkDirectory()
directory = os.path.join(os.getcwd(),sys.argv[1])
main(directory)
