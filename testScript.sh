#!/bin/bash

#running through each iteration of m input queues and n output queues each value up to 8.
#set -e
#disabled for testing but needs to be turned back on for real runs.
if [ $1 == "-q" ]
	then 
		./TestingEnvironment/Framework/framework 1 1
	else


		for ((i=1; $i < 9; i++)) ; do
			for((j=1; $j < 9; j++)) ; do
				echo $i $j
				./TestingEnvironment/Framework/framework $i $j
			done
		done
fi
