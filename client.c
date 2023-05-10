#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024
#define MSG_SIZE 500

void usage(int argc, char **argv)
{
  printf("usage: %s <server IP> <server Port>\n", argv[0]);
  printf("example: %s 127.0.0.1 51511\n", argv[0]);
  exit(EXIT_FAILURE);
}

void send_message(int sock, char *msg)
{
  // send message
  int count = send(sock, msg, strlen(msg) + 1, 0);

  if (count != strlen(msg) + 1)
    log_error("on message send");

  // waits for the full response
  memset(msg, 0, BUFSZ);
  unsigned total = 0; // chunk receiving mechanism
  while (1)
  {
    count = recv(sock, msg + total, BUFSZ - total, 0);
    if (count == 0)
      break; // connection is closed
    total += count;
  }

  printf("[conn] received %d bytes\n", count);
  puts(msg);

  close(sock);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
  if (argc == 0)
    usage(argc, argv);

  struct sockaddr_storage storage;
  if (addrparse(argv[1], argv[2], &storage) != 0)
    usage(argc, argv);

  // create socket and connect to server(by default we're using IPV4)
  int sock;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    log_error("on socket creation");
  struct sockaddr *addr = (struct sockaddr *)(&storage);

  if (connect(sock, addr, sizeof(storage)) != 0)
    log_error("on connect");

  char addrstr[BUFSZ];
  addrtostr(addr, addrstr, BUFSZ);

  printf("[conn] successfully connected to %s\n", addrstr);

  char select_command[] = "select file ";
  char send_command[] = "send file";
  char exit_command[] = "exit";

  FILE *fp;

  // user menu
  char input[BUFSZ];
  while (1)
  {
    memset(input, 0, BUFSZ);
    printf("Enter your command\n");
    fgets(input, BUFSZ - 1, stdin);
    input[strcspn(input, "\n")] = 0;
    if (strncmp(select_command, input, strlen(select_command)) == 0)
    {
      char *filename = strrchr(input, ' ');

      if (!filename)
      {
        printf("no file selected\n");
        continue;
      }

      int formats_supported = 6;
      char allowedExts[][6] = {".txt", ".c", ".cpp", ".py", ".tex", ".java"};
      int invalid = 1;
      char *ext = strrchr(input, '.');
      for (int i = 0; i < formats_supported; i++)
      {
        if (strcmp(ext, allowedExts[i]) == 0)
        {
          invalid = 0;
          break;
        }
      }
      if (invalid == 1)
      {
        printf("%s not valid!\n", filename);
        continue;
      }

      fp = fopen(filename, "r");

      if (fp == NULL) // file not found
      {
        printf("%s does not exist\n", filename);
        continue;
      }

      printf("%s selected\n", filename);
    }
    else if (strncmp(send_command, input, strlen(send_command)) == 0)
    {
      // just send
      fread(fp,MSG_SIZE)
      send_message(sock, fp);
    }
    else if (strncmp(exit_command, input, strlen(exit_command)) == 0)
    {
      // not sure if just close connection or send a message requiring so
      printf("Exit\n");
      break;
    }
  }
  exit(EXIT_SUCCESS);
}
