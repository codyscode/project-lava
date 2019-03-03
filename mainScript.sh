#!/bin/bash

displayHelp() {
	printf "-h	Print help"
	printf "-t	Run tests on all algorithms"
	printf "-w	Push benchmark results to wiki"
	printf "-p   	Push to github"	
}


testAllAlgorithms() {
	
	for dir in ./TestingEnvironment/Algorithm*/ ; do
		make -C "$dir"
		./testScript.sh
		make clean -C ./TestingEnvironment/Framework/
	done
}

pushWiki() {
	cd ..
        cd project-lava.wiki
        git pull
        cp -a  ~/project-lava/visualizations/. ~/project-lava.wiki/
        git add -A
        git commit -m "Adding run to the Database"
        git push	
}

while getopts 'twp' flag; do
	case "${flag}" in
		h) displayHelp ;;
		t) testAllAlgorithms ;;
		w) pushWiki ;;
		p) pushGithub ;;
		*) error "Unexpected option ${flag}" ;;
	esac
done

