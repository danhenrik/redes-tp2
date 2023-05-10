#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
  printf("usage: %s <version> <server Port>\n", argv[0]);
  printf("example: %s v4 51511\n", argv[0]);
  exit(EXIT_FAILURE);
}

struct thread_data
{
  int csock;
  struct sockaddr_storage storage;
};

void *client_thread(void *data)
{
  // extract the data from the object input
  struct thread_data *tdata = (struct thread_data *)data;
  struct sockaddr *caddr = (struct sockaddr *)(&(tdata->storage));
  int csock = tdata->csock;

  char caddrstr[BUFSZ];
  addrtostr(caddr, caddrstr, BUFSZ);
  printf("[inbound] connection from %s\n", caddrstr);

  char buf[BUFSZ];
  memset(buf, 0, BUFSZ);
  // message being considered is just the first receive
  size_t count = recv(csock, buf, BUFSZ, 0);

  printf("[msg rcv] %s, %d bytes: %s\n", caddrstr, (int)count, buf);

  sprintf(buf, "[ACK] remote endpoint %.1000s\n", caddrstr);

  // respond to the client
  count = send(csock, buf, strlen(buf) + 1, 0);
  if (count != strlen(buf) + 1)
    log_error("on send");

  close(csock);

  pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
  // show helper message
  if (argc == 0)
    usage(argc, argv);

  // create socket storage, store socket doesn't matter what protocol
  struct sockaddr_storage storage;
  if (server_sockaddr_init(argv[1], argv[2], &storage) != 0)
    usage(argc, argv);

  // create socket
  int s;
  s = socket(storage.ss_family, SOCK_STREAM, 0);

  if (s == -1)
    log_error("on socket creation");

  // enable rebinding to port imediatly after last instance was shut down
  int enable = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0)
    log_error("on reusage of address port");

  // start listening on port (bind to it)
  struct sockaddr *addr = (struct sockaddr *)(&storage);

  if (bind(s, addr, sizeof(storage)) != 0)
    log_error("on bind");

  if (listen(s, 10) != 0)
    log_error("on listen");

  char addrstr[BUFSZ];
  addrtostr(addr, addrstr, BUFSZ);
  printf("bound to %s, waiting connections\n", addrstr);

  while (1) // start accepting clients requests
  {
    struct sockaddr_storage cstorage;
    struct sockaddr *caddr = (struct sockaddr *)(&storage);
    socklen_t caddrlen = sizeof(cstorage);

    int csock = accept(s, caddr, &caddrlen);
    if (csock == -1)
      log_error("on accept");

    struct thread_data *tdata = malloc(sizeof(*tdata));
    if (!tdata)
      log_error("on pthread malloc");

    tdata->csock = csock;
    memcpy(&(tdata->storage), &storage, sizeof(storage));

    pthread_t tid;
    pthread_create(&tid, NULL, client_thread, tdata);
  }
  exit(EXIT_SUCCESS);
}
