#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define BUF_SZ 2048
#define MAX_MSG_SZ (BUF_SZ - 3 * sizeof(uint16_t))
#define MAX_CONNECTIONS 15
// #define DEBUG 

struct message
{
  uint16_t IdMsg;
  uint16_t IdSender;
  uint16_t IdReceiver;
  char Message[MAX_MSG_SZ];
};

struct thread_data
{
  int sock;
  struct sockaddr_storage storage;
};

void log_error(const char *msg);

int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage *storage);

void send_message(int sock, struct message *msg);

struct message *receive_message(int sockfd);