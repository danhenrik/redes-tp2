#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
  printf("usage: %s <server IP> <server Port>\n", argv[0]);
  printf("example: %s 127.0.0.1 51511\n", argv[0]);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  if (argc == 0)
    usage(argc, argv);

  struct sockaddr_storage storage;
  if (addrparse(argv[1], argv[2], &storage) != 0)
    usage(argc, argv);

  // create socket and connect to server(by default we're using IPV4)
  int s;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1)
    log_error("on socket creation");
  struct sockaddr *addr = (struct sockaddr *)(&storage);

  if (connect(s, addr, sizeof(storage)) != 0)
    log_error("on connect");

  char addrstr[BUFSZ];
  addrtostr(addr, addrstr, BUFSZ);

  printf("successfully connected to %s\n", addrstr);

  // wait for message
  char buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  printf("your message> ");
  fgets(buf, BUFSZ - 1, stdin);

  // send message
  int count = send(s, buf, strlen(buf) + 1, 0);

  if (count != strlen(buf) + 1)
    log_error("on message send");

  // waits for the full response
  memset(buf, 0, BUFSZ);
  unsigned total = 0; // chunk receiving mechanism
  while (1)
  {
    count = recv(s, buf + total, BUFSZ - total, 0);
    if (count == 0)
      break; // connection is closed
    total += count;
  }

  printf("received %d bytes\n", count);
  puts(buf);

  close(s);
  exit(EXIT_SUCCESS);
}
