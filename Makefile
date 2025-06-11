all:
	gcc -Wall -c common.c
	gcc -Wall new-client.c common.o -o new-client
	gcc -Wall server-loc.c common.o -o server-loc
	gcc -Wall server-status.c common.o -o server-status
clean:
	rm common.o new-client server-loc server-status
