#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

pthread_mutex_t conn_mutex;
pthread_mutex_t stdout_mutex;
int connections[MAX_CONNECTIONS + 1];

void usage(int argc, char **argv)
{
  printf("usage: %s <version> <server Port>\n", argv[0]);
  printf("example: %s v4 51511\n", argv[0]);
  exit(EXIT_FAILURE);
}

void broadcast_message(struct message *msg)
{
  for (int i = 1; i <= MAX_CONNECTIONS; i++)
  {
    if (connections[i] != 0)
      send_message(connections[i], msg);
  }
}

void *client_thread(void *data)
{
  printf("[inbound] moved new incoming connection to a new thread\n");

  struct thread_data *tdata = (struct thread_data *)data;
  struct sockaddr *caddr = (struct sockaddr *)(&(tdata->storage));
  int csock = tdata->sock;

  char caddrstr[BUF_SZ];
  addrtostr(caddr, caddrstr, BUF_SZ);

  pthread_mutex_lock(&stdout_mutex);
  printf("[inbound] connection from %s\n", caddrstr);
  pthread_mutex_unlock(&stdout_mutex);

  int myId = 0;

  // start listening for messages
  while (1)
  {
    struct message *msg = receive_message(csock);
    struct message res;

    // REQ_ADD
    if (msg->IdMsg == 1)
    {
      // find slot and add connection to the pool
      pthread_mutex_lock(&conn_mutex);
      for (int i = 1; i <= MAX_CONNECTIONS; i++)
      {
        if (connections[i] == 0)
        {
          connections[i] = csock;
          myId = i;
          break;
        }
      }
      pthread_mutex_unlock(&conn_mutex);

      // couldn't find a empty slot
      if (myId == 0)
      {
        res.IdMsg = 7;
        res.IdSender = 0;
        res.IdReceiver = 0;
        strcpy(res.Message, "01");
        send_message(csock, &res);
        close(csock);
        pthread_exit(NULL);
      }

      // notify everyone of the new user added
      res.IdMsg = 6;
      res.IdSender = myId;
      res.IdReceiver = 0;
      memset(res.Message, 0, MAX_MSG_SZ);
      sprintf(res.Message, "User %02d joined the group!", myId);

      for (int i = 1; i <= MAX_CONNECTIONS; i++)
      {
        if (connections[i] != 0)
          send_message(connections[i], &res);
      }

      // send connection list to new user
      res.IdMsg = 4;
      res.IdSender = 0;
      res.IdReceiver = myId;

      char connection_list[MAX_MSG_SZ];
      memset(connection_list, 0, MAX_MSG_SZ);
      char int_str[3];
      for (int j = 1; j <= MAX_CONNECTIONS; j++)
      {
        memset(int_str, 0, 3);
        if (connections[j] != 0)
        {
          sprintf(int_str, "%d", j);
          strcat(connection_list, int_str);
          strcat(connection_list, ",");
        }
      }
      connection_list[strlen(connection_list) - 1] = '\0';
      memset(res.Message, 0, MAX_MSG_SZ);
      strcpy(res.Message, connection_list);

      send_message(csock, &res);

      pthread_mutex_lock(&stdout_mutex);
      printf("User %02d added\n", myId);
      pthread_mutex_unlock(&stdout_mutex);
    }
    // REQ_REM
    else if (msg->IdMsg == 2)
    {
      pthread_mutex_lock(&conn_mutex);
      res.IdSender = 0;
      res.IdReceiver = msg->IdSender;
      // sender not found in connection pool
      if (connections[msg->IdSender] == 0)
      {
        printf("User %02d found\n", msg->IdSender);
        res.IdMsg = 7;
        strcpy(res.Message, "02");
        send_message(csock, &res);
        continue;
      }

      // send response of success and close connection
      res.IdMsg = 8;
      strcpy(res.Message, "01");
      send_message(connections[msg->IdSender], &res);
      close(connections[msg->IdSender]);

      // remove from connection pool
      connections[msg->IdSender] = 0;
      pthread_mutex_unlock(&conn_mutex);

      // notify everyone of the removal and exit thread
      broadcast_message(msg);

      pthread_mutex_lock(&stdout_mutex);
      printf("User %02d removed\n", msg->IdSender);
      pthread_mutex_unlock(&stdout_mutex);

      pthread_exit(NULL);
    }
    // MSG
    else if (msg->IdMsg == 6)
    {
      // broadcast
      if (msg->IdReceiver == 0)
      {
        res.IdMsg = msg->IdMsg;
        res.IdSender = msg->IdSender;
        res.IdReceiver = msg->IdReceiver;

        // re-assemble message
        time_t mytime;
        time(&mytime);
        struct tm now;
        localtime_r(&mytime, &now);

        // bigger to fit the whole message with metadata
        char message[3000];
        memset(message, 0, MAX_MSG_SZ);
        sprintf(message, "[%02d:%02d] %02d: %s", now.tm_hour, now.tm_min, msg->IdSender, msg->Message);
        strncpy(res.Message, message, MAX_MSG_SZ);

        // display public message on server
        pthread_mutex_lock(&stdout_mutex);
        printf("%s\n", res.Message);
        pthread_mutex_unlock(&stdout_mutex);

        // send to every socket in the connection pool but the sender
        for (int i = 1; i <= MAX_CONNECTIONS; i++)
        {
          if (connections[i] != 0 && i != msg->IdSender)
            send_message(connections[i], &res);
        }

        // send specific message to sender
        memset(message, 0, MAX_MSG_SZ);
        sprintf(message, "[%02d:%02d] -> all: %s", now.tm_hour, now.tm_min, msg->Message);
        strncpy(res.Message, message, MAX_MSG_SZ);
        send_message(connections[msg->IdSender], &res);
      }
      // unicast
      else
      {
        // receiver not found, send error message
        if (connections[msg->IdReceiver] == 0)
        {
          pthread_mutex_lock(&stdout_mutex);
          printf("User %02d not found\n", msg->IdReceiver);
          pthread_mutex_unlock(&stdout_mutex);

          res.IdMsg = 7;
          res.IdSender = 0;
          res.IdReceiver = 0;
          memset(res.Message, 0, MAX_MSG_SZ);
          strcpy(res.Message, "03");
          send_message(connections[msg->IdSender], &res);
          continue;
        }

        // re-assemble message
        time_t mytime;
        time(&mytime);
        struct tm now;
        localtime_r(&mytime, &now);

        // bigger to fit the whole message with metadata
        char message[3000];
        memset(message, 0, MAX_MSG_SZ);
        sprintf(message, "P [%02d:%02d] %02d: %s", now.tm_hour, now.tm_min, msg->IdSender, msg->Message);
        strncpy(res.Message, message, MAX_MSG_SZ);
        send_message(connections[msg->IdReceiver], &res);

        // send specific message to sender
        memset(message, 0, MAX_MSG_SZ);
        sprintf(message, "P [%02d:%02d] -> %02d: %s", now.tm_hour, now.tm_min, msg->IdSender, msg->Message);
        strncpy(res.Message, message, MAX_MSG_SZ);
        send_message(connections[msg->IdSender], &res);
      }
    }
  }
}

int main(int argc, char **argv)
{
  if (argc == 0)
    usage(argc, argv);

  struct sockaddr_storage storage;
  if (server_sockaddr_init(argv[1], argv[2], &storage) != 0)
    usage(argc, argv);

  // setup socket
  int s = socket(storage.ss_family, SOCK_STREAM, 0);

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

  if (listen(s, 15) != 0)
    log_error("on listen");

  char addrstr[BUF_SZ];
  addrtostr(addr, addrstr, BUF_SZ);
  printf("[setup] bound to %s, waiting connections\n", addrstr);

  // Initialize mutex to connection pool and stdout
  pthread_mutex_init(&conn_mutex, NULL);
  pthread_mutex_init(&stdout_mutex, NULL);

  // start accepting client requests, imediately placing each new connection in a new thread
  while (1)
  {
    struct sockaddr_storage cstorage;
    struct sockaddr *caddr = (struct sockaddr *)(&storage);
    socklen_t caddrlen = sizeof(cstorage);

    int csock = accept(s, caddr, &caddrlen);
    if (csock == -1)
      log_error("on accept");

    struct thread_data *tdata = malloc(sizeof(*tdata));
    if (!tdata)
      log_error("on pthread tdata malloc");
    tdata->sock = csock;
    memcpy(&(tdata->storage), &storage, sizeof(storage));

    pthread_t tid;
    pthread_create(&tid, NULL, client_thread, tdata);
  }

  exit(EXIT_SUCCESS);
}
