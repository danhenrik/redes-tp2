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

void send_message(int sock, char *msg)
{
  // send message
  int count = send(sock, msg, strlen(msg), 0);
  if (count != strlen(msg))
    log_error("on send");
  printf("[conn] sent message (%d bytes)\n", count);
}

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
  s = socket(storage.ss_family, SOCK_STREAM, IPPROTO_TCP);

  if (s == -1)
    log_error("on socket creation");

  // enable re-binding to port immediately after last instance was shut down
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
  printf("[setup] bound to %s, waiting connections\n", addrstr);

  char exit_command[] = "exit";

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

    char msg[MAX_MSG_SZ + 1];

    // setup file pointer and auxiliary variables
    FILE *fp = NULL;
    int is_new;
    int final_chunk = 0;
    while (1)
    {
      memset(msg, 0, MAX_MSG_SZ);
      int count = recv(csock, msg, MAX_MSG_SZ, 0);
      printf("[conn] received message (%d bytes)\n", count);

      // if msg is exit close connection from here before it send a "connection closed" message
      if (strcmp(exit_command, msg) == 0)
      {
        send_message(csock, "connection closed");
        close(csock);
        exit(EXIT_SUCCESS);
      }

      // parse header
      int filename_sz = 0;
      char *payload = strchr(msg, '.');
      if (payload != NULL)
      {
        int formats_supported = 6;
        char allowedExts[][6] = {".cpp", ".txt", ".c", ".py", ".tex", ".java"};
        for (int i = 0; i < formats_supported; i++)
        {
          if (strncmp(payload, allowedExts[i], strlen(allowedExts[i])) == 0)
          {
            filename_sz = strlen(msg) - strlen(payload) + strlen(allowedExts[i]) + 1;
            break;
          }
        }
      }

      // didnt find extension -> invalid one
      if (filename_sz == 0)
      {
        send_message(csock, "file is invalid");
        continue;
      }

      char filename[filename_sz];
      memcpy(filename, msg, filename_sz);
      filename[filename_sz - 1] = '\0';
      payload = msg + filename_sz - 1;

      // if contains end it's the last chunk, so save the flag for later and cut the tag out of the payload
      if (strstr(msg, "\\end") != NULL)
      {
        final_chunk = 1;
        payload[strlen(payload) - 4] = '\0';
      }

      // First file access, detect if is present or not in the dir to choose the message later.
      if (fp == NULL)
      {
        if (access(filename, F_OK) != -1)
          is_new = 0;
        else
          is_new = 1;
        fp = fopen(filename, "w");
      }

      // write to file
      fprintf(fp, "%s", payload);

      // respond to client at end of file
      if (final_chunk)
      {
        char end_message[MAX_MSG_SZ];
        if (is_new)
          sprintf(end_message, "file %s received", filename);
        else
          sprintf(end_message, "file %s overwritten", filename);
        send_message(csock, end_message);
        fclose(fp);
        // reset everything (able to receive the next connection)
        fp = NULL;
        final_chunk = 0;
        continue;
      }

      // if not the final chunk send a simple acknowledgment message
      send_message(csock, "ACK");
    }
  }
}
