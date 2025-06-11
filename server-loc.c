#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>

#define BUFSZ 1024

// // Estrutura padronizada para mensagens de controle
// typedef struct PeerMsg_t
// {
//     int type;       // Tipo da mensagem
//     int payload;    // Payload da mensagem (peer ID, código do erro, etc.)
//     char desc[256]; // Descrição da mensagem (erro, OK, etc)
// } PeerMsg_t;

void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <p2p server port> <clients server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51500 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

// // Função utilitária para enviar PeerMsg
// int send_msg(int sock, PeerMsg_t *msg)
// {
//     return send(sock, msg, sizeof(PeerMsg_t), 0);
// }
// // Função utilitária para receber PeerMsg
// int recv_msg(int sock, PeerMsg_t *msg)
// {
//     return recv(sock, msg, sizeof(PeerMsg_t), 0);
// }

// Gera um ID lógico simples (poderia ser incrementado ou randomizado)
int gerar_id_peer()
{
    static int id = 100;
    return id++;
}

int gerar_loc_cliente()
{
    int random = rand() % 10 + 1;
    return random;
}

int gerar_status_cliente()
{
    int random = rand() % 2;
    return random;
}

int init_clients_socket(struct sockaddr_storage *clients_storage)
{
    int s = socket(clients_storage->ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    struct sockaddr *clients_addr = (struct sockaddr *)(clients_storage);

    if (0 != bind(s, clients_addr, sizeof(*clients_storage)))
    {
        logexit("bind");
    }
    if (0 != listen(s, 10))
    {
        logexit("listen");
    }

    return s;
}

int wait_for_activity(fd_set *read_fds, int server_socket, int clients_socket, int connected_peer_id, int *client_sockets_ids, int *next_client_index, Server_Type type)
{
    FD_ZERO(read_fds);
    FD_SET(server_socket, read_fds);
    FD_SET(clients_socket, read_fds);
    FD_SET(STDIN_FILENO, read_fds);
    int max_fd = ((clients_socket > STDIN_FILENO) ? clients_socket : STDIN_FILENO) + 1;
    char buf[BUFSZ];

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets_ids[i] > 0)
        {
            FD_SET(client_sockets_ids[i], read_fds);
        }
        if (client_sockets_ids[i] > max_fd)
        {
            max_fd = client_sockets_ids[i];
        }
    }

    int rv = select(max_fd, read_fds, NULL, NULL, NULL);

    if (rv == -1)
    {
        logexit("select");
    }

    if (FD_ISSET(STDIN_FILENO, read_fds))
    {
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            if (strncmp(buf, "kill", 4) == 0)
            {
                PeerMsg_t disc = {0};
                disc.type = REQ_DISCPEER;
                disc.payload = connected_peer_id;
                send_msg(server_socket, &disc);
                recv_msg(server_socket, &disc);
                if (disc.type == OK_MSG)
                {
                    printf("%s\n", disc.desc);
                    printf("Peer %d disconnected\n", disc.payload);
                }
                else if (disc.type == ERROR_MSG)
                {
                    printf("%s\n", disc.desc);
                    logexit(disc.desc);
                }

                return 1;
            }
        }
    }

    if (FD_ISSET(server_socket, read_fds))
    {
        PeerMsg_t disc = {0};
        if (recv_msg(server_socket, &disc) == -1)
        {
            logexit(disc.desc);
            return 0;
        }

        if (disc.type == REQ_DISCPEER)
        {
            if (disc.payload != connected_peer_id)
            {
                PeerMsg_t err = {0};
                err.type = ERROR_MSG;
                strcpy(err.desc, "ERROR(02): Peer ID inválido");
                send_msg(server_socket, &err);
                return 1;
            }
            // Remove peer
            PeerMsg_t ok = {0};
            ok.type = OK_MSG;
            ok.payload = connected_peer_id;
            strcpy(ok.desc, "OK(01): Peer desconectado");
            send_msg(server_socket, &ok);
            printf("Peer %d disconnected\n", connected_peer_id);
            connected_peer_id = -1;
            return 1;
        }
        return 0; // TODO
    }

    if (FD_ISSET(clients_socket, read_fds))
    {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);
        PeerMsg_t msg = {0};

        int csock = accept(clients_socket, caddr, &caddrlen);
        if (csock == -1)
        {
            logexit("accept client");
        }
        if (*next_client_index > MAX_CLIENTS - 1)
        {
            msg.type = ERROR_MSG;
            msg.payload = CLIENT_LIMIT_ERROR;
            strcpy(msg.desc, "Clients limit exceeded");
            send_peer_msg(csock, &msg);
            printf("Azedou\n");
            close(csock);
            return 0;
        }

        msg.type = ERROR_MSG;
        msg.payload = CLIENT_LIMIT_ERROR;
        strcpy(msg.desc, "Clients limit exceeded");
        send_peer_msg(csock, &msg);

        PeerMsg_t msg = {0};
        recv_msg(csock, &msg);

        if (msg.type == REQ_CONNSEN)
        {
            if (type == LOC)
            {
                int loc = gerar_loc_cliente();
                printf("Client %d added (Loc %d)\n", msg.payload, loc);
            }
            else
            {
                int status = gerar_status_cliente();
                printf("Client %d added (%d)\n", msg.payload, status);
            }
            client_sockets_ids[*next_client_index] = msg.payload;
            *next_client_index = *next_client_index + 1;
            printf("Ok %d\n", *next_client_index);

            PeerMsg_t resp = {0};

            resp.type = RES_CONNSEN;
            resp.payload = msg.payload;
            send_msg(csock, &resp);

            return 1;
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        usage(argc, argv);
    }

    srand(time(NULL));

    struct sockaddr_storage clients_storage;
    struct sockaddr_storage p2p_storage;
    Server_Type my_type;
    int connected_peer_id = -1;
    int client_sockets[MAX_CLIENTS] = {0};

    fd_set read_fds;

    if (0 != server_sockaddr_init(argv[1], argv[2], argv[3], &p2p_storage, &clients_storage))
    {
        usage(argc, argv);
    }

    int s = socket(p2p_storage.ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    struct sockaddr *p2p_addr = (struct sockaddr *)(&p2p_storage);

    // Tenta conectar como cliente (STATUS)
    if (0 == connect(s, p2p_addr, sizeof(p2p_storage)))
    {
        my_type = STATUS;
        // Envia REQ_CONNPEER
        PeerMsg_t msg = {0};
        msg.type = REQ_CONPEER;
        send_msg(s, &msg);
        // Recebe resposta
        recv_msg(s, &msg);
        if (msg.type == 0 || msg.type == REQ_CONNSEN)
        {
            // Compatibilidade: se o outro lado não implementou, aborta
            printf("Protocolo incompatível\n");
            close(s);
            logexit("not implemented");
        }
        if (msg.type == RES_CONPEER)
        {
            printf("New Peer ID: %d\n", msg.payload);
            int peer_id_remoto = msg.payload;
            // Define seu próprio ID
            connected_peer_id = gerar_id_peer();
            PeerMsg_t resp = {0};
            resp.type = RES_CONPEER;
            resp.payload = connected_peer_id;
            send_msg(s, &resp);
            printf("Peer %d connected\n", peer_id_remoto);
        }
        else if (msg.type == ERROR_MSG)
        {
            printf("%s\n", msg.desc);
            close(s);
            logexit(msg.desc);
        }

        int csock = init_clients_socket(&clients_storage);

        int keep_loop = 1;
        int next_client_index = 0;
        while (keep_loop)
        {
            keep_loop = wait_for_activity(&read_fds, s, csock, connected_peer_id, client_sockets, &next_client_index, my_type);
        }

        // Loop de comandos
        // char comando[32];
        // while (1)
        // {
        //     printf("mensagem> ");
        //     fgets(comando, sizeof(comando), stdin);
        //     if (strncmp(comando, "kill", 4) == 0)
        //     {
        //         PeerMsg_t disc = {0};
        //         disc.type = REQ_DISCPEER;
        //         disc.payload = connected_peer_id;
        //         send_peer_msg(s, &disc);
        //         recv_peer_msg(s, &disc);
        //         if (disc.type == OK_MSG)
        //         {
        //             printf("%s\n", disc.desc);
        //             printf("Peer %d disconnected\n", disc.payload);
        //         }
        //         else if (disc.type == ERROR_MSG)
        //         {
        //             printf("%s\n", disc.desc);
        //             logexit(disc.desc);
        //         }
        //         break;
        //     }
        // }
        close(s);
        exit(EXIT_SUCCESS);
    }

    // Não encontrou peer, vira LOC e escuta
    printf("No peer found, starting to listen...\n");
    my_type = LOC;
    // int peers[MAX_PEERS] = {-1};
    if (0 != bind(s, p2p_addr, sizeof(p2p_storage)))
    {
        logexit("bind");
    }
    if (0 != listen(s, 10))
    {
        logexit("listen");
    }
    while (1)
    {
        struct sockaddr_storage s_storage;
        struct sockaddr *s_addr = (struct sockaddr *)(&s_storage);
        socklen_t s_addrlen = sizeof(s_storage);
        int s_sock = accept(s, s_addr, &s_addrlen);
        if (s_sock == -1)
        {
            logexit("accept");
        }
        PeerMsg_t msg = {0};
        recv_msg(s_sock, &msg);
        if (msg.type == REQ_CONPEER)
        {
            // Verifica limite de peers
            if (connected_peer_id != -1)
            {
                PeerMsg_t err = {0};
                err.type = ERROR_MSG;
                err.payload = PEER_LIMIT_ERROR;
                strcpy(err.desc, "Peer limit exceeded");
                send_msg(s_sock, &err);
                close(s_sock);
                continue;
            }
            // Gera ID para o peer
            connected_peer_id = gerar_id_peer();
            PeerMsg_t resp = {0};
            resp.type = RES_CONPEER;
            resp.payload = connected_peer_id;
            send_msg(s_sock, &resp);
            printf("Peer %d connected\n", connected_peer_id);
            // Recebe o ID do peer remoto
            PeerMsg_t peer_id_msg = {0};
            recv_msg(s_sock, &peer_id_msg);
            if (peer_id_msg.type == RES_CONPEER)
            {
                printf("New Peer ID: %d\n", peer_id_msg.payload);
            }

            int csock = init_clients_socket(&clients_storage);

            int keep_loop = 1;
            int next_client_index = 0;
            while (keep_loop)
            {
                keep_loop = wait_for_activity(&read_fds, s_sock, csock, connected_peer_id, client_sockets, &next_client_index, my_type);
            }

            // Aguarda REQ_DISCPEER
            // while (1)
            // {
            //     PeerMsg_t disc = {0};
            //     if (recv_peer_msg(s_sock, &disc) == -1)
            //     {
            //         logexit(disc.desc);
            //         break;
            //     }
            //     if (disc.type == REQ_DISCPEER)
            //     {
            //         if (disc.payload != connected_peer_id)
            //         {
            //             PeerMsg_t err = {0};
            //             err.type = ERROR_MSG;
            //             strcpy(err.desc, "ERROR(02): Peer ID inválido");
            //             send_peer_msg(s_sock, &err);
            //             continue;
            //         }
            //         // Remove peer
            //         PeerMsg_t ok = {0};
            //         ok.type = OK_MSG;
            //         ok.payload = connected_peer_id;
            //         strcpy(ok.desc, "OK(01): Peer desconectado");
            //         send_peer_msg(s_sock, &ok);
            //         printf("Peer %d disconnected\n", connected_peer_id);
            //         connected_peer_id = -1;
            //         break;
            //     }
            // }
            close(s_sock);
            printf("No peer found, starting to listen...\n");
        }
    }

    return 0;
}
// ...fim do arquivo...