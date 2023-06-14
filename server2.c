#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#define MAX_CLIENTS 15
#define BUFFER_SIZE 1024

int main(int argc, char **argv)
{
  int serverSocket, maxSocket, activity, i, valread, newSocket, clientSocket[MAX_CLIENTS];
  int opt = 1;
  struct sockaddr_in address;
  fd_set readfds;
  char buffer[BUFFER_SIZE];

  struct sockaddr_storage storage;
  if (server_sockaddr_init(argv[1], argv[2], &storage) != 0)
  {
    perror("error on parse params");
    exit(EXIT_FAILURE);
  }

  // Create server socket
  if ((serverSocket = socket(storage.ss_family, SOCK_STREAM, 0)) == 0)
  {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Set socket options
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt failed");
    exit(EXIT_FAILURE);
  }

  // Bind the socket to localhost port 8888
  struct sockaddr *addr = (struct sockaddr *)(&storage);
  if (bind(serverSocket, addr, sizeof(storage)) < 0)
  {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  printf("Listening on port 8888...\n");

  // Listen for incoming connections
  if (listen(serverSocket, 15) < 0)
  {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  // Initialize client sockets
  for (i = 0; i < MAX_CLIENTS; i++)
  {
    clientSocket[i] = 0;
  }

  while (1)
  {
    FD_ZERO(&readfds);

    // Add server socket to set
    FD_SET(serverSocket, &readfds);
    maxSocket = serverSocket;

    // Add client sockets to set
    for (i = 0; i < MAX_CLIENTS; i++)
    {
      int socketFD = clientSocket[i];
      if (socketFD > 0)
      {
        FD_SET(socketFD, &readfds);
      }
      if (socketFD > maxSocket)
      {
        maxSocket = socketFD;
      }
    }

    // Wait for activity on any socket
    activity = select(maxSocket + 1, &readfds, NULL, NULL, NULL);
    if (activity < 0)
    {
      perror("select error");
      exit(EXIT_FAILURE);
    }

    // New incoming connection
    if (FD_ISSET(serverSocket, &readfds))
    {
      struct sockaddr_storage cstorage;
      struct sockaddr *caddr = (struct sockaddr *)(&storage);
      socklen_t caddrlen = sizeof(cstorage);
      if ((newSocket = accept(serverSocket, caddr, &caddrlen)) < 0)
      {
        perror("accept error");
        exit(EXIT_FAILURE);
      }

      // Add new socket to array of client sockets
      for (i = 0; i < MAX_CLIENTS; i++)
      {
        if (clientSocket[i] == 0)
        {
          clientSocket[i] = newSocket;
          printf("New client connected: socket fd is %d\n", newSocket);
          break;
        }
      }
    }

    // Check for client activity
    for (i = 0; i < MAX_CLIENTS; i++)
    {
      int socketFD = clientSocket[i];
      if (FD_ISSET(socketFD, &readfds))
      {
        // Handle data received from client
        valread = read(socketFD, buffer, BUFFER_SIZE);
        if (valread == 0)
        {
          // Client disconnected
          struct sockaddr *caddr = (struct sockaddr *)(&address);
          getpeername(socketFD, caddr, (socklen_t *)&address);
          printf("Client disconnected: socket fd is %d, IP address is %s, port is %d\n",
                 socketFD, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

          // Close the socket and remove from the client sockets array
          close(socketFD);
          clientSocket[i] = 0;
        }
        else
        {
          // Process the received message
          printf("Received message from client %d: %s\n", socketFD, buffer);

          // Broadcast the message to other clients
          for (int j = 0; j < MAX_CLIENTS; j++)
          {
            int otherSocketFD = clientSocket[j];
            if (otherSocketFD != 0 && otherSocketFD != socketFD)
            {
              send(otherSocketFD, buffer, valread, 0);
            }
          }
        }
      }
    }
  }

  // Close server socket
  close(serverSocket);

  return 0;
}
