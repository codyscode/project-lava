# Testing Environment  
-Used to test algorithms.    
-Each algorithm has its own folder with a make file similar to the one in the directory algorithm1/ and the algorithm.c folder.  
    1. The make file in each algorithmx (x >= 1) folder makes the binary file (algorithm.o)   
    2. Moves the file to the framework folder   
    3. Changes to the framework folder   
    4. Runs make   

# Testing (How to Run)  
-As of now to run a specific algorithm:  
    1. Switch to that algorithms folder and run: make  
    2. Switch to /Framework directory and run ./Framework x y  
        -x and y are integers between 1 and 8 (inclusive) 

-Ideally we have an upper level make file that does the following:     
    1. For each algorithm directory we have a corresponding line that does the following:  
        a. Call make (we end up in the /Framework directory)  
            -Compile the framework with the specific algorithm  
        b. run the testing script for all cases  
            -Run tests to output to .csv file  
        c. call make clean (in /Framework directory)  
            -Because the .o file is the only thing in the /Framework directory it is cleaned up by clean and this allows the framework to reset fully  
    2. Call Anand's python script to deal with output   
-Each time we had an algorithm we would add another line that does step 1 to the top level make file  

# Files in /Framework  
-The framework files are the files from /FrameworkMod    
-Except for algorithm.h, Do not update /Framework files only.  
-Instead, update files in /FrameworkMod and then move them to /Framework.  
