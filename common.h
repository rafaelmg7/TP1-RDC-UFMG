#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

typedef enum {
    LOC,
    STATUS
} Server_Type;

#define MAX_PEERS 2 

void logexit(const char *msg);

int addrparse(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *addrstr, const char *portp2pstr, const char *portstr,
                         struct sockaddr_storage *p2p_storage, struct sockaddr_storage *clients_storage);
