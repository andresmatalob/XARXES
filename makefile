all:
	-gcc -ansi -pedantic -Wall -std=c17 client.c -o client
clean:
	rm -fr client
