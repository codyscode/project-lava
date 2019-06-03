main: wrapper.o main.o
	gcc -o ./bin/queue ./bin/wrapper.o ./bin/main.o -pthread

wrapper.o: ./src/wrapper.c ./src/scope.h
	gcc -c ./src/wrapper.c -o ./bin/wrapper.o -pthread

main.o: ./src/main.c 
	gcc -c ./src/main.c -o ./bin/main.o -pthread
