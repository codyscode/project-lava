#!/bin/bash

displayHelp() {
	echo "-h	Print help"
	echo "-t	Run tests on all algorithms"
	echo "-w	Push benchmark results to wiki"	
}


testAllAlgorithms() {
	
	for dir in ./TestingEnvironment/Algorithm*/ ; do
		make -C "$dir"
		sudo ./testScript.sh
		make clean -C ./TestingEnvironment/Framework/
	done
}

pushWiki() {
	cd ..
        cd project-lava.wiki
        git pull
        mv  ~/project-lava/TestingEnvironment/Framework/output.csv ~/project-lava.wiki/Data/
        git add -A
        git commit -m "Adding run to the Database"
        git push	
}

while getopts 'htwp' flag; do
	case "${flag}" in
		h) displayHelp ;;
		t) testAllAlgorithms ;;
		w) pushWiki ;;
		*) error "Unexpected option ${flag}" ;;
	esac
done

