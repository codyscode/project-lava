LDFLAGS=-pthread

initial-queuecode: queuecode.o
	$(clang) $(LDFLAGS) -o queuecode queuecode.o

clean:
	rm -f queuecode.o
