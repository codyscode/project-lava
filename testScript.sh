#! /bin/bash
export PATH=~/project-lava/TestingEnivironment
echo $PATH
#set -e
make -C $PATH

for ((i=1; $i < 9; i++)) ; do
	for((j=1; $j < 9; j++)) ; do
		$PATH/framework $i $j
	done
done
