#!/bin/bash

for dir in ./TestingEnvironment/Algorithm*/ ; do
	make -C "$dir"
	./testScript.sh
	make clean -C ./TestingEnvironment/Framework/
done


