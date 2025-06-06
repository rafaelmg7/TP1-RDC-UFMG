all:
	gcc -Wall -c common.c
	gcc -Wall client.c common.o -o client
	gcc -Wall server-loc.c common.o -o server-loc
	gcc -Wall server-status.c common.o -o server-status
clean:
	rm common.o client server-loc server-status
