all: 
	gcc -Wall -c common.c
	gcc -Wall -g client.c common.o -o client
	gcc -Wall -g server.c common.o -o server

clean:
	rm common.o client server server-mt