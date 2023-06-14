all: 
	gcc -Wall -c common.c
	gcc -Wall -pthread user.c common.o -o user
	gcc -Wall -pthread server.c common.o -o server