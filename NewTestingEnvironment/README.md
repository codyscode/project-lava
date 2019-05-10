# Testing Environment  
- Used to test algorithms.    
- Each algorithm has its own folder with a make file similar to the one in the directory algorithm1/ and the algorithm.c folder.  
    1. The make file in each algorithmx (x >= 1) folder makes the binary file (algorithm.o)   
    2. Moves the file to the framework folder   
    3. Changes to the framework folder   
    4. Runs make   

# Testing (How to Run)  
- As of now to run a specific algorithm:  
    1. Switch to that algorithms folder and run: make  
    2. Switch to /Framework directory and run ./Framework x y  
        -x and y are integers between 1 and 8 (inclusive) 
    Or
    1. Call ./mainScript.sh -s "algorithm name"

-To run all algorithms  
    1. go to upper level directory in repo and call ./mainScript.h [args]  
    2. [args]:  
  <ul>  <li>   -h 	     Print help</li>
      <ul><li>        - This command will bring up this list.</li></ul>
	<li>    -t	     Run tests on all algorithms</li>
      <ul><li>          - This will automatically make and build all algorithms.c files in Algorithm*/ folders in the same folder as the mainScript.sh</li></ul>
	<li>    -q       runs quick test on all algorithms</li>
      <ul><li>          - This will automatically make and build all algorithms.c files in Algorithm*/ folders in the same folder as the mainScript.sh but only the M = 1 N = 1 iteration</li></ul>
	<li>    -w	     Push benchmark results to wiki</li>
      <ul><li>          - Pushes the results to the project-lava.wiki repo which should exist along side the project-lava repo</li></ul>
	<li>    -v       Turns CSV files into visualizations</li>
      <ul><li>          - Should be done after a successful test run, do not use with -r   </li></ul>           
	<li>    -s  "Algorithm#/"    Will run test on a specific algorithm only</li>
      <ul><li>          - A search will be done in all the Algorithm*/ folders for an algorithm.c file with the name Defined in it. The CSV file will be pushed to the wiki.</li></ul>
      <li>    -s  "Algorithm#/ m n"    Will run test on a specific algorithm and only on the iteration m n</li>
      <ul><li>          - A search will be done in all the Algorithm*/ folders for an algorithm.c file with the name Defined in it. The CSV file will be removed.</li></ul>
    <li>    -r		 Will run all test on all algorithms 10 times and run visualizations each time</li>
      <ul><li>          - This test runs -t version 10 times, it also takes care of moving files to the project-lava.wiki and calling the visualization script.</li></ul>
 </ul>
