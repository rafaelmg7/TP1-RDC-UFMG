#pragma once

#include <stdlib.h>
#include <arpa/inet.h>

#define BUFSZ 501

typedef enum
{
    LOC,
    STATUS
} Server;

typedef enum {
    SERVER_SHUTDOWN,
    CONTINUE_RUNNING,
    TERMINATE_P2P_CONNECTION,
} ServerCommand;

typedef struct Msg
{
    int type;
    int payload;
    char desc[BUFSZ];
} Msg_t;

typedef struct Client{
    int id;
    int socket_id;
    int data; // Location or status 
} Client_t;

#define MAX_PEERS 2
#define MAX_CLIENTS 15

#define REQ_CONPEER      20
#define RES_CONPEER      21
#define REQ_DISCPEER     22
#define REQ_CONNSEN      23
#define RES_CONNSEN      24
#define REQ_DISCSEN      25

#define REQ_CHECKALERT   36
#define RES_CHECKALERT   37
#define REQ_SENSLOC      38
#define RES_SENSLOC      39
#define REQ_SENSSTATUS   40
#define RES_SENSSTATUS   41
#define REQ_LOCLIST      42
#define RES_LOCLIST      43

#define ERROR_MSG        255
#define OK_MSG           0
#define PEER_LIMIT_ERROR 1
#define CLIENT_LIMIT_ERROR 9

#define DESC_ERROR_01 "Peer limit exceeded"
#define DESC_ERROR_02 "Peer not found"
#define DESC_ERROR_09 "Sensor limit exceeded"
#define DESC_ERROR_10 "Sensor not found"
#define DESC_ERROR_11 "Location not found"

#define DESC_OK_01 "Successful disconnect"
#define DESC_OK_02 "Successful create"
#define DESC_OK_03 "Status do sensor 0"

void logexit(const char *msg);

int client_sockaddr_init(const char *addrstr, const char *portstr, const char *portstr_2,
                         struct sockaddr_storage *storage, struct sockaddr_storage *storage_2);

int server_sockaddr_init(const char *addrstr, const char *portp2pstr, const char *portstr,
                         struct sockaddr_storage *p2p_storage, struct sockaddr_storage *clients_storage);

int send_msg(int sock, Msg_t *msg);

int recv_msg(int sock, Msg_t *msg);

void toLowerString(char *str);