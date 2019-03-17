#!/bin/bash

#help menu to detail how the program works
displayHelp() {
	echo "-h	Print help"
	echo "-t	Run tests on all algorithms"
	echo "-q        runs quick test on all algorithms"
	echo "-w	Push benchmark results to wiki"	
}

#runs all 64 iteration of each algorithm being made
testAllAlgorithms() {
	echo ">>>>>>>> RUNNING FULL TEST <<<<<<<<"
	for dir in ./TestingEnvironment/Algorithm*/ ; do
		make -C "$dir"
		 ./testScript.sh
		make clean -C ./TestingEnvironment/Framework/
	done
}

#quicktestAllAlgorithms only runs one iteration of each algorithm m = 4 n = 4. This is for testing purposes and the data should not be pushed unless it is being used for testing.
quicktestAllAlgorithms() {
	echo ">>>>>>>> RUNNING QUICK TEST <<<<<<<<<"
        for dir in ./TestingEnvironment/Algorithm*/ ; do
                make -C "$dir"
                 ./testScript.sh -q
                make clean -C ./TestingEnvironment/Framework/
        done
	rm -f *.csv
}

#moves the csv files generated by the framework to the git wiki then pushes them as well as updates the markdown file by calling a function to do so.
pushWiki() {
	now="$(date +'%d_%m_%Y_%H_%M')"
        echo $now
        for _file in *.csv ; do
                mv $_file "$now$_file"
                echo $_file
                mv  ./$now$_file ../project-lava.wiki/Data/
        done
	cd ..
        cd project-lava.wiki
	fileStructure_markdown
	NON_ROOT_USER=$(who am i | awk '{print $1}');
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

#takes flags and runs the appropriate function
while getopts 'htqwp' flag; do
	case "${flag}" in
		h) displayHelp ;;
		t) testAllAlgorithms ;;
		q) quicktestAllAlgorithms ;;
		w) pushWiki ;;
		*) error "Unexpected option ${flag}" ;;
	esac
done

