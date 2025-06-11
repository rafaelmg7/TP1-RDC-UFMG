#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

typedef enum
{
    LOC,
    STATUS
} Server_Type;

typedef struct PeerMsg_t
{
    int type;       // Tipo da mensagem
    int payload;    // Payload da mensagem (peer ID, código do erro, etc.)
    char desc[256]; // Descrição da mensagem (erro, OK, etc)
} PeerMsg_t;

#define MAX_PEERS 2
#define MAX_CLIENTS 2

#define REQ_CONPEER 20
#define RES_CONPEER 21
#define REQ_DISCPEER 22
#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define REQ_DISCSEN 25

#define ERROR_MSG 255
#define OK_MSG 0
#define PEER_LIMIT_ERROR 1
#define CLIENT_LIMIT_ERROR 9

void logexit(const char *msg);

int client_sockaddr_init(const char *addrstr, const char *portstr, const char *portstr_2,
                         struct sockaddr_storage *storage, struct sockaddr_storage *storage_2);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *addrstr, const char *portp2pstr, const char *portstr,
                         struct sockaddr_storage *p2p_storage, struct sockaddr_storage *clients_storage);

int send_msg(int sock, PeerMsg_t *msg);

int recv_msg(int sock, PeerMsg_t *msg);

int gerar_id_cliente();
