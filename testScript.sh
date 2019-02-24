#! /bin/bash

for ((i=1; $i < 9; i++)) ; do
	for((j=1; $j < 9; j++)) ; do
		./TestingEnvironment/Framework/framework $i $j
	done
done
