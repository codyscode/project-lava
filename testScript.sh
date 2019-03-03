#!/bin/bash
#Set -e should stop the runs if any of the framework runs error out. 
#The framework will be doing the writing out.
#After the run this needs to push to git Wiki.
#current usage format will be testscript.sh -w <filename>
#This will look for a file in the project-lava directory with the arg2 name and move it to the
#the wiki to be pushed
#current usage format will be testscript.sh -w
#This will copy the files to the wiki repo and they will be pushed from there.
#Added sudo to the test run since we are assigning threads to cores.


#set -e
#disabled for testing but needs to be turned back on for real runs.

for ((i=1; $i < 9; i++)) ; do
	for((j=1; $j < 9; j++)) ; do
		echo $i $j
		./TestingEnvironment/Framework/framework $i $j
	done
done

