#include "common.h"

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

  struct in6_addr inaddr6;
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
    {
      log_error("on IPV4 ntop");
      exit(EXIT_FAILURE);
    }
    port = ntohs(addr4->sin_port);
  }
  else if (addr->sa_family == AF_INET6) // IPV6
  {
    version = 6;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
    if (!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN + 1))
    {
      log_error("on IPV6 ntop");
      exit(EXIT_FAILURE);
    }
    port = ntohs(addr6->sin6_port);
  }
  else
  {
    log_error("unknown protocol family");
    exit(EXIT_FAILURE);
  }

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

void send_message(int sockfd, struct message *msg)
{
#ifdef DEBUG
  printf(">>>>>\n{\n\tIdMsg: %02d,\n\tIdSender: %d,\n\tIdReceiver: %d,\n\tMessage: %s\n}\n>>>>>\n", msg->IdMsg, msg->IdSender, msg->IdReceiver, msg->Message);
#endif

  if (send(sockfd, msg, BUF_SZ, 0) == -1)
    log_error("on send");
}

struct message *receive_message(int sockfd)
{
  char *buf = malloc(BUF_SZ);
  memset(buf, 0, BUF_SZ);
  if (recv(sockfd, buf, BUF_SZ, 0) == -1)
    log_error("on recv");
  struct message *msg = (struct message *)buf;

#ifdef DEBUG
  printf("<<<<<\n{\n\tIdMsg: %02d,\n\tIdSender: %d,\n\tIdReceiver: %d,\n\tMessage: %s\n}\n<<<<<\n", msg->IdMsg, msg->IdSender, msg->IdReceiver, msg->Message);
#endif

  return msg;
}