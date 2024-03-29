#!/bin/bash
OPTION_VAL=""
#running through each iteration of m input queues and n output queues each value up to 8.
#set -e
#disabled for testing but needs to be turned back on for real runs.

#Runs only the 1:1 iteration of each algorithm in the TestingEnvironment
quickTest(){
	./framework 1 1 i
}
#runs through all iterations
normalTest(){
	for ((i=1; $i < 9; i++)) ; do
			for((j=1; $j < 9; j++)) ; do
				echo $i $j
				./framework $i $j i
			done
		done
}
#runs through a specfic iteration
specificTest(){
	count=0
	for opt in $OPTION_VAL; do
		if [ "$count" -eq 0 ]
		then 
			inputT="$opt"
			count=$((count+1))
		elif [ "$count" -eq 1 ]
		then 
			outputT="$opt"
			count=$((count+1))
		fi
	done
	printf "\n>>>>>>>>>> INPUT: $inputT AND OUTPUT: $outputT <<<<<<<<<<<<\n"
	./framework "$inputT" "$outputT" i
}
#Handles the arguments
while getopts 'qns:' flag; do
	case "${flag}" in
		q) quickTest ;;
		n) normalTest ;;
		s) OPTION_VAL=$OPTARG
			specificTest ;;
		*) error "Unexpected option ${flag}" ;;
	esac
done