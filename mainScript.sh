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
	now="$(date +'%d_%m_%Y_%H:%M')"
        echo $now
        for _file in *.csv ; do
                mv $_file "$now$_file"
                echo $_file
                mv  ./$now$_file ~/project-lava.wiki/Data/
        done
	cd ..
        cd project-lava.wiki
        git pull
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

