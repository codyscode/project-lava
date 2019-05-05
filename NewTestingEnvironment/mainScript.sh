#!/bin/bash

OPTION_VAL=""
#help menu to detail how the program works
displayHelp() {
	echo "-h 	     Print help"
	echo "-t	     Run tests on all algorithms"
	echo "-q         runs quick test on all algorithms"
	echo "-w	     Push benchmark results to wiki"	
	echo "-v         Turns CSV files into visualizations do not use with -r"
	echo "-s al_name Will run test on a specific algorithm only"
	echo "-r		 Will run all test on all algorithms 10 times and run visualizations each time"
}
#Function that checks if the framework is already running on the server.
check_isRunning(){
	SERVICE="framework"
	if pgrep -x "$SERVICE"
	then
    		echo "$SERVICE is running"
    		exit 1
	else
    		echo "$SERVICE is not running starting testing"
	fi
}
testSpecificAlgorithm(){
	check_isRunning
	NON_ROOT_USER=$(who am i | awk '{print $1}');
	echo "running test on Algorithm: "$OPTION_VAL
	for dir in Algorithm*/ ; do
		cd $dir
		temp=$(grep "#define ALGNAME *" algorithm.c | awk '{print $3}')
		temp="${temp#\"}"
		temp="${temp%\"}"
		echo "Algorithm in current folder: $temp  Algorithm you are trying to run: $OPTION_VAL"

		if [ "$temp" == "$OPTION_VAL" ]
		then
			echo "in the if statement"
			cd ..
			make AP="$dir"
			./testScript.sh
			make AP="$dir" clean
		fi
		 cd /home/$NON_ROOT_USER/project-lava/NewTestingEnvironment/
	done
}
runVisualization()
{
	NON_ROOT_USER=$(who am i | awk '{print $1}');
	cd home/$NON_ROOT_USER/project-lava.wiki/Data/
	python /home/$NON_ROOT_USER/project-lava/NewTestingEnvironment/visualization.py
}

#runs all 64 iteration of each algorithm being made
testAllAlgorithms() {
	check_isRunning
	echo ">>>>>>>> RUNNING FULL TEST <<<<<<<<"
	for dir in Algorithm*/ ; do
		make AP="$dir"
		 ./testScript.sh
		make AP="$dir" clean
	done
	echo ">>>>>>>> FULL TEST COMPLETED <<<<<<<<<"
}

#quicktestAllAlgorithms only runs one iteration of each algorithm m = 4 n = 4. This is for testing purposes and the data should not be pushed unless it is being used for testing.
quicktestAllAlgorithms() {
	check_isRunning
	echo ">>>>>>>> RUNNING QUICK TEST <<<<<<<<<"
        for dir in Algorithm*/ ; do
			make AP="$dir"
		 	./testScript.sh -q
			make AP="$dir" clean
        done
	rm -f *.csv
	echo ">>>>>>>> QUICK TEST COMPLETED <<<<<<<<<"
}

#moves the csv files generated by the framework to the git wiki then pushes them as well as updates the markdown file by calling a function to do so.
pushWiki() {
	NON_ROOT_USER=$(who am i | awk '{print $1}');
	now="$(date +'%d_%m_%Y_%H_%M')"
        echo $now
        for _file in *.csv ; do
                mv $_file "$now$_file"
                echo $_file
                mv  ./$now$_file /home/$NON_ROOT_USER/project-lava.wiki/Data/
        done
	# cd ..
    #     cd project-lava.wiki
	sudo -u $NON_ROOT_USER cd /home/$NON_ROOT_USER/project-lava.wiki/
	fileStructure_markdown
        echo $NON_ROOT_USER
        sudo -u $NON_ROOT_USER git pull
        sudo -u $NON_ROOT_USER git add -A
        sudo -u $NON_ROOT_USER git commit -m "Adding run to the Database and updating file structure"
        sudo -u $NON_ROOT_USER git push	
}

#Function to update the file_structure.md file on the git wiki so that we can display the files and directories there using tree and some formatting.
fileStructure_markdown(){
	tree > file_structure.md
	sed -i 's|[`\'']|\||g' file_structure.md
	sed -i'' 's|$|<br>|' file_structure.md	
}
#Function to run the test script on each algorithm 10 times and run the visualization script each time.
repeatedRuns(){
	NON_ROOT_USER=$(who am i | awk '{print $1}');
	for i in 1 2 3 4 5 6 7 8 9 10; do
		testAllAlgorithms
		cd /home/$NON_ROOT_USER/project-lava/NewTestingEnvironment/
		pushWiki
	done
	runVisualization
}

#takes flags and runs the appropriate function
while getopts 'htqws:vrp' flag; do
	case "${flag}" in
		h) displayHelp ;;
		t) testAllAlgorithms ;;
		q) quicktestAllAlgorithms ;;
		w) pushWiki ;;
		s) OPTION_VAL=$OPTARG
		   testSpecificAlgorithm 
		   ;;
		v) runVisualization ;;
		r) repeatedRuns ;;
		*) error "Unexpected option ${flag}" ;;
	esac
done


