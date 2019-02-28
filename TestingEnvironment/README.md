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

-To run all algorithms
    1. go to upper level directory in repo and call ./mainScript.h [args]
    2. [args]:
        a. -p   push to the git repo
        b. -w   push output files to git wiki
