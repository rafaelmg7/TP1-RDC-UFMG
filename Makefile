all:
	gcc -Wall -c common.c
	gcc -Wall sensor.c common.o -o sensor
	gcc -Wall server-loc.c common.o -o server-loc
clean:
	rm common.o sensor server-loc *.txt
