#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SZ 1024
#define MAX_MSG_SZ 500

void usage(int argc, char **argv)
{
  printf("usage: %s <server IP> <server Port>\n", argv[0]);
  printf("example: %s 127.0.0.1 51511\n", argv[0]);
  exit(EXIT_FAILURE);
}

void send_message(int sock, char *msg)
{
  // send message
  int count = send(sock, msg, strlen(msg), 0);
  if (count != strlen(msg))
    log_error("on send");
  printf("[conn] sent message (%d bytes)\n", count);

  // waits for the response
  memset(msg, 0, MAX_MSG_SZ);

  count = recv(sock, msg, MAX_MSG_SZ, 0);

  printf("[conn] received \"%s\" (%d bytes)\n", msg, count);
}

int main(int argc, char **argv)
{
  if (argc == 0)
    usage(argc, argv);

  // \================= configure and stablish connection ================/

  struct sockaddr_storage storage;
  if (addrparse(argv[1], argv[2], &storage) != 0)
    usage(argc, argv);

  // create socket and connect to server
  int sock;
  int sock4 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  int sock6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

  if (sock4 == -1)
    log_error("on v4 socket creation");
  if (sock6 == -1)
    log_error("on v6 socket creation");

  struct sockaddr *addr = (struct sockaddr *)(&storage);

  // Try to connect using both protocols (v4 & v6)
  int connected4 = connect(sock4, addr, sizeof(storage));
  int connected6 = connect(sock6, addr, sizeof(storage));

  if (connected4 == 0)
    sock = sock4;
  else if (connected6 == 0)
    sock = sock6;
  else
    log_error("on connect");

  char addrstr[BUF_SZ];
  addrtostr(addr, addrstr, BUF_SZ);

  printf("[conn] successfully connected to %s\n", addrstr);

  // \====================== build user menu =============================/

  char select_command[] = "select file ";
  char send_command[] = "send file";
  char exit_command[] = "exit";

  FILE *fp = NULL;

  char input[BUF_SZ];
  int header_sz = MAX_MSG_SZ;
  char header[header_sz];
  memset(header, 0, MAX_MSG_SZ);
  char endFlag[] = "\\end";
  int endFlag_sz = strlen(endFlag);
  long eof_offset;

  while (1)
  {
    memset(input, 0, BUF_SZ);
    printf("Enter your command\n");
    fgets(input, BUF_SZ - 1, stdin);
    input[strcspn(input, "\n")] = 0;
    // Select File CMD
    if (strncmp(select_command, input, strlen(select_command)) == 0)
    {
      // Extract filename from command
      char *filename = strrchr(input, ' ');
      filename = filename + 1;
      if (!filename)
      {
        printf("no file selected\n");
        continue;
      }

      // Validate if file extension is acceptable
      int formats_supported = 6;
      char allowedExts[][6] = {".txt", ".c", ".cpp", ".py", ".tex", ".java"};
      int invalid = 1;
      char *ext = strrchr(input, '.');
      if (ext != NULL)
      {
        for (int i = 0; i < formats_supported; i++)
        {
          if (strcmp(ext, allowedExts[i]) == 0)
          {
            invalid = 0;
            break;
          }
        }
      }
      if (invalid == 1)
      {
        printf("%s not valid!\n", filename);
        continue;
      }

      // Open the file
      fp = fopen(filename, "r");

      if (fp == NULL) // File not found
      {
        printf("%s does not exist\n", filename);
        continue;
      }

      // Get offset to EOF (so we know when to stop)
      fseek(fp, 0, SEEK_END);
      eof_offset = ftell(fp);
      fseek(fp, 0, SEEK_SET);

      // Create the header (filename)
      sprintf(header, "%s", filename);
      header_sz = strlen(header);
      printf("%s selected\n", filename);
    }
    // Send File CMD
    else if (strncmp(send_command, input, strlen(send_command)) == 0)
    {
      // Check if there's a file selected
      if (fp == NULL)
      {
        printf("no file selected!\n");
        continue;
      }

      // Compute payload size based on header size
      int payload_sz = MAX_MSG_SZ - header_sz;
      char payload[payload_sz + 1];
      char msg[MAX_MSG_SZ];
      int last_chunk = 0;

      // send the file in chunks
      while (1)
      {
        // reset memory so we dont send garbage through
        memset(payload, 0, payload_sz);
        memset(msg, 0, MAX_MSG_SZ);

        // If the end is less than the remaining content simply send the remaining content with the endflag (end of file)
        if (eof_offset < payload_sz - endFlag_sz) // last chunk
        {
          last_chunk = 1;
          payload_sz = eof_offset + endFlag_sz;
        }

        // read from file
        fread(payload, sizeof(payload[0]), payload_sz, fp);
        payload[payload_sz] = 0;

        // If it's the end of the file add the \end flag else just send the chunk.
        if (last_chunk)
          sprintf(msg, "%s%s\\end", header, payload);
        else
          sprintf(msg, "%s%s", header, payload);
        send_message(sock, msg);

        // Subtract the amount of data sent from the EOF tracker
        eof_offset -= strlen(payload);

        // Loop stop condition
        if (last_chunk)
          break;
      }

      // reset everything
      fclose(fp);
      fp = NULL;
    }
    // Exit CMD
    else if (strncmp(exit_command, input, strlen(exit_command)) == 0)
    {
      // Send exit command to server
      send_message(sock, exit_command);
      exit(EXIT_SUCCESS);
      break;
    }
  }
}
