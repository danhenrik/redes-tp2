all: 
	gcc -Wall -g -c common.c
	gcc -Wall -g -pthread user.c common.o -o user
	gcc -Wall -g -pthread server.c common.o -o server