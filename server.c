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

int getConnectionsCount()
{
  int count = 0;
  for (int i = 1; i <= MAX_CONNECTIONS; i++)
  {
    if (connections[i] != 0)
      count++;
  }
  return count;
}

void broadcast_user_list()
{
  struct message notification;
  notification.IdMsg = 4;
  notification.IdSender = 0;

  char connection_list[MAX_MSG_SZ];
  memset(connection_list, 0, MAX_MSG_SZ);
  for (int j = 1; j <= MAX_CONNECTIONS; j++)
  {
    if (connections[j] != 0)
    {
      char int_str[2];
      sprintf(int_str, "%d", j);
      strcat(connection_list, int_str);
      strcat(connection_list, ",");
    }
  }
  connection_list[strlen(connection_list) - 1] = '\0';
  memset(notification.Message, 0, MAX_MSG_SZ);
  strcpy(notification.Message, connection_list);

  // Notify everyone
  for (int j = 1; j <= MAX_CONNECTIONS; j++)
  {
    if (connections[j] != 0)
    {
      notification.IdReceiver = j;
      send_message(connections[j], &notification);
    }
  }
}

void *client_thread(void *data)
{
  printf("[inbound] moved connection to new thread\n");
  // extract the data from the object input
  struct thread_data *tdata = (struct thread_data *)data;
  struct sockaddr *caddr = (struct sockaddr *)(&(tdata->storage));
  int csock = tdata->sock;

  char caddrstr[BUF_SZ];
  addrtostr(caddr, caddrstr, BUF_SZ);

  pthread_mutex_lock(&stdout_mutex);
  printf("[inbound] connection from %s\n", caddrstr);
  pthread_mutex_unlock(&stdout_mutex);

  int myId = 0;

  while (1)
  {
    struct message *msg = receive_message(csock);

    // CTRL-C on client
    if (msg->IdMsg == 0)
    {
      pthread_mutex_lock(&stdout_mutex);
      printf("User with id %02d quit, stopping client thread\n", myId);
      pthread_mutex_unlock(&stdout_mutex);

      pthread_mutex_lock(&conn_mutex);
      connections[myId] = 0;
      pthread_mutex_unlock(&conn_mutex);

      broadcast_user_list();

      pthread_exit(NULL);
    }
    // REQ_ADD
    if (msg->IdMsg == 1)
    {
      // check if there's space for new connections

      if (getConnectionsCount() >= MAX_CONNECTIONS)
      {
        struct message res;
        res.IdMsg = 7;
        res.IdSender = 0;
        res.IdReceiver = 0;
        strcpy(res.Message, "01");
        send_message(csock, &res);
        close(csock);
        pthread_exit(NULL);
      }

      int userAdded = 0;
      for (int i = 1; i <= MAX_CONNECTIONS; i++)
      {
        if (connections[i] == 0)
        {
          // Add connection to the pool
          pthread_mutex_lock(&conn_mutex);
          connections[i] = csock;
          pthread_mutex_unlock(&conn_mutex);
          userAdded = i;
          myId = i;
          break;
        }
      }

      // Build response message with connnections list
      struct message res;
      res.IdMsg = 4;
      res.IdSender = 0;
      res.IdReceiver = userAdded;

      char connection_list[MAX_MSG_SZ];
      memset(connection_list, 0, MAX_MSG_SZ);
      for (int j = 1; j <= MAX_CONNECTIONS; j++)
      {
        if (connections[j] != 0)
        {
          char int_str[2];
          sprintf(int_str, "%d", j);
          strcat(connection_list, int_str);
          strcat(connection_list, ",");
        }
      }
      connection_list[strlen(connection_list) - 1] = '\0';
      memset(res.Message, 0, MAX_MSG_SZ);
      strcpy(res.Message, connection_list);

      send_message(csock, &res);

      broadcast_user_list();
      pthread_mutex_lock(&stdout_mutex);
      printf("User %02d added\n", userAdded);
      pthread_mutex_unlock(&stdout_mutex);
    }
    // REQ_REM
    else if (msg->IdMsg == 2)
    {
      struct message res;
      res.IdSender = 0;
      res.IdReceiver = msg->IdSender;
      if (connections[msg->IdSender] != 0)
      {
        // send response of success and close connection
        res.IdMsg = 8;
        strcpy(res.Message, "01");
        send_message(connections[msg->IdSender], &res);
        close(connections[msg->IdSender]);

        // remove from connection pool
        pthread_mutex_lock(&conn_mutex);
        connections[msg->IdSender] = 0;
        pthread_mutex_unlock(&conn_mutex);

        broadcast_user_list();

        pthread_mutex_lock(&stdout_mutex);
        printf("User %02d removed\n", msg->IdSender);
        fflush(stdout);
        pthread_mutex_unlock(&stdout_mutex);

        pthread_exit(NULL);
      }
      else
      {
        // send response of failure
        res.IdMsg = 7;
        strcpy(res.Message, "02");
        send_message(csock, &res);
      }
    }
    // MSG
    else if (msg->IdMsg == 6)
    {
      // broadcast
      if (msg->IdReceiver == 0)
      {
        // send to every socket in the connection pool
        for (int i = 1; i <= MAX_CONNECTIONS; i++)
        {
          if (connections[i] != 0)
            send_message(connections[i], msg);
        }

        // log message
        time_t mytime;
        time(&mytime);
        struct tm now;
        localtime_r(&mytime, &now);
        pthread_mutex_lock(&stdout_mutex);
        printf("[%02d:%02d] %02d: %s\n", now.tm_hour, now.tm_min, msg->IdSender, msg->Message);
        pthread_mutex_unlock(&stdout_mutex);
      }
      // unicast
      else
      {
        if (connections[msg->IdReceiver] != 0)
          send_message(connections[msg->IdReceiver], msg);
        else
        {
          pthread_mutex_lock(&stdout_mutex);
          printf("User %02d not found\n", msg->IdReceiver);
          pthread_mutex_unlock(&stdout_mutex);

          // Receiver not found, send error message
          struct message err_msg;
          err_msg.IdMsg = 7;
          err_msg.IdSender = 0;
          err_msg.IdReceiver = 0;
          strcpy(err_msg.Message, "03");
          send_message(connections[msg->IdSender], &err_msg);
        }
      }
    }
  }
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

  // Initialize mutex to connection pool
  pthread_mutex_init(&conn_mutex, NULL);
  pthread_mutex_init(&stdout_mutex, NULL);

  while (1) // start accepting clients requests, once at a time
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
    tdata->tid = tid;

    pthread_create(&tid, NULL, client_thread, tdata);
  }

  exit(EXIT_SUCCESS);
}
