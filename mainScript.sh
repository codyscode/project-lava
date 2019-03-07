#!/bin/bash

displayHelp() {
	echo "-h	Print help"
	echo "-t	Run tests on all algorithms"
	echo "-w	Push benchmark results to wiki"	
}


testAllAlgorithms() {
	
	for dir in ./TestingEnvironment/Algorithm*/ ; do
		make -C "$dir"
		 ./testScript.sh
		make clean -C ./TestingEnvironment/Framework/
	done
}

pushWiki() {
	now="$(date +'%d_%m_%Y_%H:%M')"
        echo $now
        for _file in *.csv ; do
                mv $_file "$now$_file"
                echo $_file
                mv  ./$now$_file ../project-lava.wiki/Data/
        done
	cd ..
        cd project-lava.wiki
	NON_ROOT_USER=$(who am i | awk '{print $1}');
        echo $NON_ROOT_USER
        sudo -u $NON_ROOT_USER  git pull
        sudo -u $NON_ROOT_USER git add -A
        sudo -u $NON_ROOT_USER git commit -m "Adding run to the Database"
        sudo -u $NON_ROOT_USER git push	
}

while getopts 'htwp' flag; do
	case "${flag}" in
		h) displayHelp ;;
		t) testAllAlgorithms ;;
		w) pushWiki ;;
		*) error "Unexpected option ${flag}" ;;
	esac
done

