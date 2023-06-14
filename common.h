#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_MSG_SZ 2042
#define BUF_SZ 2048
#define MAX_CONNECTIONS 3

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
  void (*callback)(char *);
  pthread_t tid;
};

void log_error(const char *msg);

int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage *storage);

char *serialize(struct message *msg);

void send_message(int sock, struct message *msg);

struct message *deserialize(char *buf);

struct message *receive_message(int sockfd);