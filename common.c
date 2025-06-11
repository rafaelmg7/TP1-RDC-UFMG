#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include "common.h"

void logexit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int client_sockaddr_init(const char *addrstr, const char *portstr, const char *portstr_2,
              struct sockaddr_storage *storage, struct sockaddr_storage *storage_2)
{
    if (addrstr == NULL || portstr == NULL || portstr_2 == NULL)
    {
        return -1;
    }

    uint16_t port = (uint16_t)atoi(portstr); // unsigned short
    uint16_t port_2 = (uint16_t)atoi(portstr_2);

    if (port == 0 || port_2 == 0)
    {
        return -1;
    }

    port = htons(port); // host to network short
    port_2 = htons(port_2);

    struct in_addr inaddr4; // 32-bit IP address
    struct in_addr inaddr4_2;
    if (inet_pton(AF_INET, addrstr, &inaddr4) && inet_pton(AF_INET, addrstr, &inaddr4_2))
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        struct sockaddr_in *addr4_2 = (struct sockaddr_in *)storage_2;

        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;

        addr4_2->sin_family = AF_INET;
        addr4_2->sin_port = port_2;
        addr4_2->sin_addr = inaddr4_2;

        return 0;
    }

    return -1;
}

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize)
{
    int version;
    char addrstr[INET6_ADDRSTRLEN + 1] = "";
    uint16_t port;

    if (addr->sa_family == AF_INET)
    {
        version = 4;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        if (!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr,
                       INET6_ADDRSTRLEN + 1))
        {
            logexit("ntop");
        }
        port = ntohs(addr4->sin_port); // network to host short
    }
    else if (addr->sa_family == AF_INET6)
    {
        version = 6;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        if (!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr,
                       INET6_ADDRSTRLEN + 1))
        {
            logexit("ntop");
        }
        port = ntohs(addr6->sin6_port); // network to host short
    }
    else
    {
        logexit("unknown protocol family.");
    }
    if (str)
    {
        snprintf(str, strsize, "IPv%d %s %hu", version, addrstr, port);
    }
}

int server_sockaddr_init(const char *addrstr, const char *portp2pstr, const char *portstr,
                         struct sockaddr_storage *p2p_storage, struct sockaddr_storage *clients_storage)
{
    if (addrstr == NULL || portp2pstr == NULL || portstr == NULL)
    {
        return -1;
    }

    uint16_t p2p_port = (uint16_t)atoi(portp2pstr);  // unsigned short
    uint16_t clients_port = (uint16_t)atoi(portstr); // unsigned short

    if (clients_port == 0 || p2p_port == 0)
    {
        return -1;
    }

    p2p_port = htons(p2p_port);         // host to network short
    clients_port = htons(clients_port); // host to network short

    struct in_addr inaddr4;        // 32-bit IP address
    struct in_addr inaddr4_client; // 32-bit IP address
    if (inet_pton(AF_INET, addrstr, &inaddr4) && inet_pton(AF_INET, addrstr, &inaddr4_client))
    {
        struct sockaddr_in *p2p_addr4 = (struct sockaddr_in *)p2p_storage;
        struct sockaddr_in *client_addr4 = (struct sockaddr_in *)clients_storage;

        p2p_addr4->sin_family = AF_INET;
        p2p_addr4->sin_port = p2p_port;
        p2p_addr4->sin_addr = inaddr4;

        client_addr4->sin_family = AF_INET;
        client_addr4->sin_port = clients_port;
        client_addr4->sin_addr = inaddr4_client;
        return 0;
    }

    return -1;
}

// Função utilitária para enviar PeerMsg
int send_msg(int sock, PeerMsg_t *msg)
{
    return send(sock, msg, sizeof(PeerMsg_t), 0);
}
// Função utilitária para receber PeerMsg
int recv_msg(int sock, PeerMsg_t *msg)
{
    return recv(sock, msg, sizeof(PeerMsg_t), 0);
}

int gerar_id_cliente()
{
    static int ids[MAX_CLIENTS] = {0}; // 0 means unused
    static int count = 0;

    if (count >= MAX_CLIENTS)
    {
        // All IDs used
        return -1;
    }

    int id;
    int unique = 0;

    while (!unique)
    {
        id = rand() % MAX_CLIENTS + 1; // id in [1, MAX_CLIENTS]
        unique = 1;
        for (int i = 0; i < count; i++)
        {
            if (ids[i] == id)
            {
                unique = 0;
                break;
            }
        }
    }

    ids[count++] = id;
    return id;
}
