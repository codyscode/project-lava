#!/bin/bash

doGitStuff() {

	echo ThisIsAPlaceHolder

}


testAllAlgorithms() {
	
	for dir in ./TestingEnvironment/Algorithm*/ ; do
		make -C "$dir"
		./testScript.sh
		make clean -C ./TestingEnvironment/Framework/
	done
}


if [ "$1" = "t" ]; then
	testAllAlgorithms
else
	doGitStuff
fi

