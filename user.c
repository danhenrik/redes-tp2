#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

int connections[MAX_CONNECTIONS + 1];
int myId = 0;

void usage(int argc, char **argv)
{
  printf("usage: %s <server IP> <server Port>\n", argv[0]);
  printf("example: %s 127.0.0.1 51511\n", argv[0]);
  exit(EXIT_FAILURE);
}

void *listener_thread(void *data)
{
  printf("[info] Listening for incoming messages on separate thread\n");

  struct thread_data *tdata = (struct thread_data *)data;
  int sock = tdata->sock;

  while (1)
  {
    struct message *msg = receive_message(sock);

    // CTRL-C (SIGINT) on server
    if (msg->IdMsg == 0)
    {
      printf("Server closed, stopping execution\n");
      exit(EXIT_FAILURE);
    }
    // connections list update (both add and remove)
    else if (msg->IdMsg == 4)
    {
      // Parse the list of connections received as string like if it was an array (separated by comma)
      int length = strlen(msg->Message);
      char n_str[2] = "";
      for (int i = 0; i <= length; i++)
      {
        if (msg->Message[i] == ',' || length == i)
        {
          int userId = atoi(n_str);
          if (connections[userId] == 0 || connections[userId] == 3)
          {
            connections[userId] = 1;
            printf("User %02d joined the group!\n", userId);
          }
          // flag connections in the list as 2 so later we can distinguish them from removed connections,
          // those still marked with 1
          if (connections[userId] == 1)
            connections[userId] = 2;

          memset(n_str, 0, 2);
          continue;
        }
        strncat(n_str, &msg->Message[i], 1);
      }

      // remove connections that are not in the list
      for (int i = 0; i <= MAX_CONNECTIONS; i++)
      {
        // as we flagged in the iteration over the list, connections marked with 1 are not in the updated list
        if (connections[i] == 1)
        {
          connections[i] = 0;
          printf("User %02d left the group!\n", i);
        }
        // set the flag back to 1 for the next update
        else if (connections[i] == 2)
          connections[i] = 1;
      }
    }
    // receive text message (broadcast or unicast)
    else if (msg->IdMsg == 6)
    {
      time_t mytime;
      time(&mytime);
      struct tm now;
      localtime_r(&mytime, &now);

      // broadcasted by me
      if (msg->IdSender == myId && msg->IdReceiver == 0)
        printf("[%02d:%02d] -> all: %s\n", now.tm_hour, now.tm_min, msg->Message);
      // broadcasted to me
      else if (msg->IdReceiver == 0)
        printf("[%02d:%02d] %02d: %s\n", now.tm_hour, now.tm_min, msg->IdSender, msg->Message);
      // privately sent by me
      else if (msg->IdSender == myId && msg->IdReceiver != 0)
        printf("P [%02d:%02d] -> %02d: %s\n", now.tm_hour, now.tm_min, msg->IdReceiver, msg->Message);
      // privately sent to me
      else if (msg->IdReceiver == myId)
        printf("P [%02d:%02d] %02d: %s\n", now.tm_hour, now.tm_min, msg->IdSender, msg->Message);
    }
    // error messages
    else if (msg->IdMsg == 7)
    {
      if (strcmp(msg->Message, "02") == 0)
        printf("User not found\n");
      if (strcmp(msg->Message, "03") == 0)
        printf("Receiver not found\n");
    }
    // success on close connection
    else if (msg->IdMsg == 8)
    {
      printf("Removed Successfully\n");
      exit(EXIT_SUCCESS);
    }
  }
}

int main(int argc, char **argv)
{
  if (argc == 0)
    usage(argc, argv);

  // configure and stablish connection
  struct sockaddr_storage storage;
  if (addrparse(argv[1], argv[2], &storage) != 0)
    usage(argc, argv);

  int sock = socket(storage.ss_family, SOCK_STREAM, 0);

  struct sockaddr *addr = (struct sockaddr *)(&storage);

  if (connect(sock, addr, sizeof(storage)) != 0)
    log_error("on connect");

  char addrstr[BUF_SZ];
  addrtostr(addr, addrstr, BUF_SZ);
  printf("[conn] successfully connected to %s\n", addrstr);

  // Request to be added to group
  struct message req;
  req.IdMsg = 1;
  req.IdSender = 0;
  req.IdReceiver = 0;
  strcpy(req.Message, "");

  printf("sending request to be added to group\n");
  send_message(sock, &req);

  struct message *res = receive_message(sock);
  // successfully connected
  if (res->IdMsg == 4)
  {
    int length = strlen(res->Message);
    char n_str[2] = "";
    for (int i = 0; i <= length; i++)
    {
      if (res->Message[i] == ',' || length == i)
      {
        int userId = atoi(n_str);
        if (res->IdReceiver == userId)
          connections[userId] = 3;
        else
          connections[userId] = 1;
        memset(n_str, 0, 2);
        continue;
      }
      strncat(n_str, &res->Message[i], 1);
    }
    myId = res->IdReceiver;
  }
  else if (res->IdMsg == 7)
  {
    // Quit if unable to connect
    if (strcmp(res->Message, "01") == 0)
    {
      printf("User limit exceeded\n");
      exit(EXIT_FAILURE);
    }
  }

  // start the thread to receive messages in paralel
  struct thread_data *tdata = malloc(sizeof(*tdata));
  if (!tdata)
    log_error("on pthread malloc");
  tdata->sock = sock;
  memcpy(&(tdata->storage), &storage, sizeof(storage));

  pthread_t tid;
  pthread_create(&tid, NULL, listener_thread, tdata);

  // start accepting user input
  char send_all_command[] = "send all \"";
  char send_to_command[] = "send to ";
  char list_users_command[] = "list users";
  char close_conn_command[] = "close connection";

  char input[BUF_SZ];
  while (1)
  {
    printf("Enter your command...\n");
    memset(input, 0, BUF_SZ);
    fgets(input, BUF_SZ - 1, stdin);
    input[strcspn(input, "\n")] = 0;

    // send all {message}
    if (strncmp(send_all_command, input, strlen(send_all_command)) == 0)
    {
      // extract message from command
      char *message = input + strlen(send_all_command);
      const char *end = strstr(message, "\"");

      if (message == end)
      {
        printf("The message is empty!\n");
        continue;
      }
      else if (end == NULL)
      {
        printf("The message is invalid!\n");
        continue;
      }

      message[end - message] = '\0';

      if (strlen(message) > MAX_MSG_SZ)
      {
        printf("The message is too big!\n");
        continue;
      }

      // create the message and send it
      struct message msg;
      msg.IdMsg = 6;
      msg.IdSender = myId;
      msg.IdReceiver = 0;
      memset(msg.Message, 0, MAX_MSG_SZ);
      strcpy(msg.Message, message);

      send_message(sock, &msg);
    }
    // send to {id} {message}
    else if (strncmp(send_to_command, input, strlen(send_to_command)) == 0)
    {
      // extract receiverId from command
      char receiverId_str[2];
      strncpy(receiverId_str, input + strlen(send_to_command), 2);

      int receiverId = atoi(receiverId_str);

      if (!receiverId)
      {
        printf("invalid user id\n");
        continue;
      }

      // extract message from command
      char *message = strstr(input, "\"") + 1;
      const char *end = strstr(message, "\"");

      if (message == end)
      {
        printf("The message is empty!\n");
        continue;
      }
      else if (end == NULL)
      {
        printf("The message is invalid!\n");
        continue;
      }

      message[end - message] = '\0';

      if (strlen(message) > MAX_MSG_SZ)
      {
        printf("The message is too big!\n");
        continue;
      }

      // create the message and send it
      struct message msg;
      msg.IdMsg = 6;
      msg.IdSender = myId;
      msg.IdReceiver = receiverId;
      memset(msg.Message, 0, MAX_MSG_SZ);
      strcpy(msg.Message, message);

      send_message(sock, &msg);
    }
    // list users
    else if (strncmp(list_users_command, input, strlen(list_users_command)) == 0)
    {
      // Print the list (saved locally) excluding myself 
      for (int i = 1; i <= MAX_CONNECTIONS; i++)
      {
        if (connections[i] == 1 && myId != i)
          printf("%02d ", i);
      }
      printf("\n");
    }
    // close connection
    else if (strncmp(close_conn_command, input, strlen(close_conn_command)) == 0)
    {
      // Send close connection message to server
      struct message req;
      req.IdMsg = 2;
      req.IdSender = myId;
      req.IdReceiver = 0;
      strcpy(req.Message, "");

      send_message(sock, &req);
      // let response be handled by listener thread
    }
    else
    {
      printf("Invalid command\nTry one of the following:\n - send all \"message\"\n - send to <receiverId> \"message\"\n - list users\n - close connection\n");
    }
  }
}