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
    Or
    1. Call ./mainScript.sh -s "algorithm name"

-To run all algorithms  
    1. go to upper level directory in repo and call ./mainScript.h [args]  
    2. [args]:  
        -h 	     Print help
                - This command will bring up this list.
	    -t	     Run tests on all algorithms
                - This will automatically make and build all algorithms.c files in Algorithm*/ folders in the same folder as the mainScript.sh
	    -q       runs quick test on all algorithms
                - This will automatically make and build all algorithms.c files in Algorithm*/ folders in the same folder as the mainScript.sh but only the M = 1 N = 1 iteration
	    -w	     Push benchmark results to wiki
                - Pushes the results to the project-lava.wiki repo which should exist along side the project-lava repo
	    -v       Turns CSV files into visualizations
                - Should be done after a successful test run, do not use with -r              
	    -s  "al_name"    Will run test on a specific algorithm only
                - A search will be done in all the Algorithm*/ folders for an algorithm.c file with the name Defined in it.
        -r		 Will run all test on all algorithms 10 times and run visualizations each time
                - This test runs -t version 10 times, it also takes care of moving files to the project-lava.wiki and calling the visualization script.
