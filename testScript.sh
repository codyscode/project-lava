#Set -e should stop the runs if any of the framework runs error out. 
#The framework will be doing the writing out.
#After the run this needs to push to git Wiki.
#current usage format will be testscript.sh -w <filename>
#This will look for a file in the project-lava directory with the arg2 name and move it to the
#the wiki to be pushed
#current usage format will be testscript.sh -w
#This will copy the files to the wiki repo and they will be pushed from there.
#Added sudo to the test run since we are assigning threads to cores.


#set -e
set -e
#disabled for testing but needs to be turned back on for real runs.

for ((i=1; $i < 9; i++)) ; do
	for((j=1; $j < 9; j++)) ; do
		sudo	./TestingEnvironment/Framework/framework $i $j
	done
done

if [ $1 == 'w' ]
 then
	cd ..
	cd project-lava.wiki
	git pull
	cp ~/project-lava/$2 ~/project-lava.wiki/
	git add $2
        cp -a  ~/project-lava/visualizations/. ~/project-lava.wiki/
        git add -A
	git commit -m "Adding run to the Database"
	git push
fi
