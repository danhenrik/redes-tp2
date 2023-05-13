#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define BUF_SZ 1024
#define MAX_MSG_SZ 500

void usage(int argc, char **argv)
{
  printf("usage: %s <version> <server Port>\n", argv[0]);
  printf("example: %s v4 51511\n", argv[0]);
  exit(EXIT_FAILURE);
}

// void recv_message()

int main(int argc, char **argv)
{
  // show helper message
  if (argc == 0)
    usage(argc, argv);

  // create socket storage, store socket doesn't matter what protocol it is
  struct sockaddr_storage storage;
  if (server_sockaddr_init(argv[1], argv[2], &storage) != 0)
    usage(argc, argv);

  // create socket
  int s;
  s = socket(storage.ss_family, SOCK_STREAM, 0);

  if (s == -1)
    log_error("on socket creation");

  // enable re-binding to port imediatly after last instance was shut down
  int enable = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0)
    log_error("on reusage of address port");

  // bind to port and start listening
  struct sockaddr *addr = (struct sockaddr *)(&storage);

  if (bind(s, addr, sizeof(storage)) != 0)
    log_error("on bind");

  if (listen(s, 10) != 0)
    log_error("on listen");

  char addrstr[BUF_SZ];
  addrtostr(addr, addrstr, BUF_SZ);
  printf("bound to %s, waiting connections\n", addrstr);

  while (1) // start accepting clients requests, once at a time
  {
    struct sockaddr_storage cstorage;
    struct sockaddr *caddr = (struct sockaddr *)(&storage);
    socklen_t caddrlen = sizeof(cstorage);

    int csock = accept(s, caddr, &caddrlen);

    if (csock == -1)
      log_error("on accept");

    char caddrstr[BUF_SZ];
    addrtostr(caddr, caddrstr, BUF_SZ);
    printf("[inbound] connection from %s\n", caddrstr);

    char msg[MAX_MSG_SZ];

    FILE *fp;

    while (1) // End of string != /end
    {
      memset(msg, 0, MAX_MSG_SZ);
      int count = recv(csock, msg, MAX_MSG_SZ, 0);
      // if msg is exit close connection from here before it sending a "connection closed" message (TODO)

      // parse header and filename
      char *payload = strchr(msg, ' ') + 1;
      int payload_sz = strlen(payload);

      int filename_sz = strlen(msg) - payload_sz - 1;
      char filename[filename_sz];
      memcpy(filename, msg, filename_sz);

      // Get rid of first " " in payload
      memcpy(msg, payload + 1, payload_sz);
      *msg = *msg - filename_sz;

      // if the msg doesn't finish with a /end return error receiving file "filename"
      if (strstr(msg, "/end") == NULL)
      {
        char error_response[MAX_MSG_SZ];
        sprintf(error_response, "error receiving file %s”", filename);
        count = send(csock, error_response, strlen(error_response), 0);
      }
      // strcmp(strrchr(msg, '/'), "end");

      // else get rid of /end flag, write to file and go for next chunk
      *msg = *msg - strlen("/end");
      fp = fopen(filename, "a");
      fwrite(msg, strlen(msg), 0, fp);

      count = send(csock, "ACK", strlen("ACK"), 0);
      if (count != strlen("ACK"))
        log_error("on send");
    }
  }
}
