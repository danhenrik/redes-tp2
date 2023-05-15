#include "common.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

void log_error(const char *msg)
{
  char error[] = "error ";
  perror(strcat(error, msg));
  exit(EXIT_FAILURE);
}

// -1 -> invalid parse. 0 -> success
int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage)
{
  if (addrstr == NULL || portstr == NULL)
    return -1;

  uint16_t port = (uint16_t)atoi(portstr);
  if (port == 0)
    return -1;

  port = htons(port); // host to network short

  struct in_addr inaddr4;
  if (inet_pton(AF_INET, addrstr, &inaddr4)) // 32 bit ipv4 addr
  {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
    addr4->sin_family = AF_INET;
    addr4->sin_port = port;
    addr4->sin_addr = inaddr4;
    return 0;
  }

  struct in_addr inaddr6;
  if (inet_pton(AF_INET6, addrstr, &inaddr6)) // 128 bit ipv6 addr
  {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = port;
    memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));
    return 0;
  }
  return -1;
}

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize)
{
  int version;
  char addrstr[INET6_ADDRSTRLEN + 1] = "";
  uint16_t port;

  if (addr->sa_family == AF_INET) // IPV4
  {
    version = 4;
    struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
    if (!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr, INET_ADDRSTRLEN + 1))
      log_error("on IPV4 ntop");
    port = ntohs(addr4->sin_port);
  }
  else if (addr->sa_family == AF_INET6) // IPV6
  {
    version = 6;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
    if (!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN + 1))
      log_error("on IPV6 ntop");
    port = ntohs(addr6->sin6_port);
  }
  else
    log_error("unknown protocol family");

  if (str)
    snprintf(str, strsize, "IPV%d %s %hu", version, addrstr, port);
}

int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage *storage)
{
  uint16_t port = (uint16_t)atoi(portstr);

  if (port == 0)
    return -1;
  port = htons(port);

  memset(storage, 0, sizeof(*storage));

  if (strcmp(proto, "v4") == 0) // socket is ipv4
  {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
    addr4->sin_family = AF_INET;
    addr4->sin_port = port;
    addr4->sin_addr.s_addr = INADDR_ANY;
    return 0;
  }
  else if (strcmp(proto, "v6") == 0) // socket is ipv6
  {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = port;
    addr6->sin6_addr = in6addr_any;
    return 0;
  }
  return -1;
}