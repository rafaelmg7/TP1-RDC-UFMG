#pragma once

#include <stdlib.h>
#include <arpa/inet.h>

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

typedef struct PeerMsg
{
    int type;       // Tipo da mensagem
    int payload;    // Payload da mensagem (peer ID, código do erro, etc.)
    char desc[256]; // Descrição da mensagem (erro, OK, etc)
} PeerMsg_t;

typedef struct Client{
    int id;
    int socket_id;
    int data; // Location or status 
} Client_t;

#define MAX_PEERS 2
#define MAX_CLIENTS 4

// Message codes
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

// Message descriptions
#define DESC_REQ_CONPEER      "Requisição de conexão entre peers"
#define DESC_RES_CONPEER      "Resposta de conexão entre peers"
#define DESC_REQ_DISCPEER     "Requisição de desconexão entre peers"
#define DESC_REQ_CONNSEN      "Requisição de conexão de sensor"
#define DESC_RES_CONNSEN      "Resposta de conexão de sensor"
#define DESC_REQ_DISCSEN      "Requisição de desconexão de sensor"

#define DESC_REQ_CHECKALERT   "Requisição de alerta de status positivo de pane na rede (SensID)"
#define DESC_RES_CHECKALERT   "Resposta de alerta de status positivo de pane na rede (LocID)"
#define DESC_REQ_SENSLOC      "Requisição de informação sobre a localização de um sensor (SensID)"
#define DESC_RES_SENSLOC      "Resposta de informação sobre a localização de um sensor (LocID)"
#define DESC_REQ_SENSSTATUS   "Requisição de status de sensor (SensID)"
#define DESC_RES_SENSSTATUS   "Resposta de status de sensor (StatusID)"
#define DESC_REQ_LOCLIST      "Requisição de sensores presentes em uma localização (LocId)"
#define DESC_RES_LOCLIST      "Resposta de sensores presentes em uma localização (SenIDs)"

void logexit(const char *msg);

int client_sockaddr_init(const char *addrstr, const char *portstr, const char *portstr_2,
                         struct sockaddr_storage *storage, struct sockaddr_storage *storage_2);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *addrstr, const char *portp2pstr, const char *portstr,
                         struct sockaddr_storage *p2p_storage, struct sockaddr_storage *clients_storage);

int send_msg(int sock, PeerMsg_t *msg);

int recv_msg(int sock, PeerMsg_t *msg);

int gerar_id_cliente();

void toLowerString(char *str);