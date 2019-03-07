#!/bin/bash

#running through each iteration of m input queues and n output queues each value up to 8.

#current usage format will be testscript.sh 

for ((i=1; $i < 9; i++)) ; do
	for((j=1; $j < 9; j++)) ; do
		echo $i $j
		./TestingEnvironment/Framework/framework $i $j
	done
done

