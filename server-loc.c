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

void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <p2p server port> <clients server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51500 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

// Gera um ID lógico simples (poderia ser incrementado ou randomizado)
int gerar_id_peer(int other_id)
{
    static int id = 100;
    if (id != other_id)
    {
        return id++;
    }

    return ++id;
}

int get_client_loc()
{
    int random = rand() % 10 + 1;
    return random;
}

int get_client_status()
{
    int random = rand() % 2;
    return random;
}

void init_passive_server(int s, struct sockaddr *p2p_addr, struct sockaddr_storage p2p_storage)
{
    if (0 != bind(s, p2p_addr, sizeof(p2p_storage)))
    {
        logexit("bind");
    }
    if (0 != listen(s, 10))
    {
        logexit("listen");
    }
}

int handle_peer_accept(int s, int *connected_peer_id, int *server_sock)
{
    int my_peer_id = 0;
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
        if (*connected_peer_id != -1)
        {
            PeerMsg_t err = {0};
            err.type = ERROR_MSG;
            err.payload = PEER_LIMIT_ERROR;
            strcpy(err.desc, "ERROR(01): Peer limit exceeded");
            send_msg(s_sock, &err);
            close(s_sock);
            return 0;
        }

        *connected_peer_id = gerar_id_peer(0);
        PeerMsg_t resp = {0};
        resp.type = RES_CONPEER;
        resp.payload = *connected_peer_id;
        send_msg(s_sock, &resp);
        printf("Peer %d connected\n", *connected_peer_id);

        PeerMsg_t peer_id_msg = {0};
        recv_msg(s_sock, &peer_id_msg);
        if (peer_id_msg.type == RES_CONPEER)
        {
            my_peer_id = peer_id_msg.payload;
            printf("New Peer ID: %d\n", my_peer_id);
        }
    }

    *server_sock = s_sock;
    return my_peer_id;
}

int start_active_socket(int s, int *connected_peer_id)
{
    int my_peer_id;

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
        my_peer_id = msg.payload;
        // Define ID do outro
        *connected_peer_id = gerar_id_peer(my_peer_id);
        PeerMsg_t resp = {0};
        resp.type = RES_CONPEER;
        resp.payload = *connected_peer_id;
        send_msg(s, &resp);
        printf("Peer %d connected\n", *connected_peer_id);
    }
    else if (msg.type == ERROR_MSG)
    {
        close(s);
        logexit(msg.desc);
    }

    return my_peer_id;
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

ServerCommand handle_server_checkalert(int peer_socket, Client_t client,
                                       int *next_client_index, Server type)
{
    printf("REQ_CHECKALERT %d\n", client.id);

    PeerMsg_t msg = {0};
    msg.type = RES_CHECKALERT;
    msg.payload = client.data;
    strcpy(msg.desc, DESC_RES_CHECKALERT);
    send_msg(peer_socket, &msg);
    return CONTINUE_RUNNING;

    // if(client.data != 0){
    // }

    // return CONTINUE_RUNNING;
}

/**
 * Handles user input from stdin
 */
ServerCommand handle_stdin_input(char *buf, int server_socket, int my_peer_id)
{
    memset(buf, 0, BUFSZ);
    if (fgets(buf, BUFSZ, stdin) != NULL)
    {
        if (strncmp(buf, "kill", 4) == 0)
        {
            PeerMsg_t disc = {0};
            disc.type = REQ_DISCPEER;
            disc.payload = my_peer_id;

            // Send disconnect request
            send_msg(server_socket, &disc);

            // Wait for response
            memset(&disc, 0, sizeof(disc));
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

            return SERVER_SHUTDOWN;
        }
    }
    return CONTINUE_RUNNING;
}

/**
 * Handles peer-to-peer socket communication
 */
ServerCommand handle_peer_activity(int server_socket, int *connected_peer_id, Client_t *clients)
{
    PeerMsg_t disc = {0};
    int count = recv_msg(server_socket, &disc);

    if (count <= 0)
    {
        printf("Peer %d connection lost.\n", *connected_peer_id);
        *connected_peer_id = -1;
        return TERMINATE_P2P_CONNECTION;
    }

    if (disc.type == REQ_DISCPEER)
    {
        if (disc.payload != *connected_peer_id)
        {
            PeerMsg_t err = {0};
            err.type = ERROR_MSG;
            strcpy(err.desc, "ERROR(02): Peer ID inválido");
            send_msg(server_socket, &err);
            return CONTINUE_RUNNING;
        }

        PeerMsg_t ok = {0};
        ok.type = OK_MSG;
        ok.payload = *connected_peer_id;
        strcpy(ok.desc, "OK(01): Peer desconectado");
        send_msg(server_socket, &ok);
        printf("Peer %d disconnected\n", *connected_peer_id);

        *connected_peer_id = -1;
        return TERMINATE_P2P_CONNECTION;
    }

    if (disc.type == REQ_CHECKALERT)
    {
        Client_t client = {0};
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (disc.payload == clients[i].id)
            {
                client = clients[i];
                return handle_server_checkalert(server_socket, client, NULL, LOC);
            }
        }

        PeerMsg_t err = {0};
        err.type = ERROR_MSG;
        err.payload = 10;
        strcpy(err.desc, "ERROR(10): Sensor not found");
        send_msg(server_socket, &err);

        return CONTINUE_RUNNING;
    }

    return CONTINUE_RUNNING;
}

ServerCommand handle_client_connection(int clients_socket, Client_t *clients,
                                       int *next_client_index, fd_set *read_fds, Server type)
{
    struct sockaddr_storage cstorage;
    socklen_t caddrlen = sizeof(cstorage);
    PeerMsg_t msg = {0};

    int csock = accept(clients_socket, (struct sockaddr *)(&cstorage), &caddrlen);
    if (csock == -1)
    {
        logexit("accept client");
    }

    recv_msg(csock, &msg);

    if (msg.type == REQ_CONNSEN)
    {
        if (*next_client_index > MAX_CLIENTS - 1)
        {
            PeerMsg_t err = {0};
            err.type = ERROR_MSG;
            err.payload = CLIENT_LIMIT_ERROR;
            strcpy(err.desc, "Clients limit exceeded");
            send_msg(csock, &err);
            close(csock);
            return CONTINUE_RUNNING;
        }

        PeerMsg_t resp = {0};
        int client_data;
        if (type == LOC)
        {
            client_data = get_client_loc();
            memcpy(resp.desc, "SL", 2);
            printf("Client %d added (Loc %d)\n", msg.payload, client_data);
        }
        else
        {
            client_data = get_client_status();
            memcpy(resp.desc, "SS", 2);
            printf("Client %d added (%d)\n", msg.payload, client_data);
        }

        clients[*next_client_index].id = msg.payload;
        clients[*next_client_index].socket_id = csock;
        clients[*next_client_index].data = client_data;
        (*next_client_index)++;

        resp.type = RES_CONNSEN;
        resp.payload = msg.payload;
        send_msg(csock, &resp);
    }

    return CONTINUE_RUNNING;
}

ServerCommand handle_req_discsen(int current_socket, Client_t *clients,
                                 int *next_client_index, int client_index, Server type)
{

    clients[client_index].socket_id = 0;
    clients[client_index].id = 0;
    clients[client_index].data = 0;

    for (int k = client_index; k < *next_client_index - 1; k++)
    {
        clients[k] = clients[k + 1];
    }
    (*next_client_index)--;

    PeerMsg_t ok = {0};
    ok.type = OK_MSG;
    ok.payload = 1;
    sprintf(ok.desc, "%s Successful disconnect", type == LOC ? "SL" : "SS");
    send_msg(current_socket, &ok);
    printf("Client %d removed\n", clients[client_index].id);

    return CONTINUE_RUNNING;
}

ServerCommand handle_req_sensstatus(int current_socket, int peer_socket, Client_t client,
                                    int *next_client_index, Server type)
{
    printf("REQ_SENSSTATUS %d\n", client.id);
    PeerMsg_t msg = {0};

    if (client.data != 1)
    {
        msg.type = OK_MSG;
        msg.payload = 2;
        strcpy(msg.desc, "OK(03): Sensor working");
        send_msg(current_socket, &msg);
        return CONTINUE_RUNNING;
    }

    msg.type = REQ_CHECKALERT;
    msg.payload = client.id;
    strcpy(msg.desc, DESC_REQ_CHECKALERT);
    send_msg(peer_socket, &msg);

    memset(&msg, 0, sizeof(msg));

    recv_msg(peer_socket, &msg);

    PeerMsg_t resp = {0};

    if (msg.type == ERROR_MSG)
    {
        resp.type = ERROR_MSG;
        resp.payload = 10;
        strcpy(resp.desc, "ERROR(10): Sensor not found");
    }

    if (msg.type == RES_CHECKALERT)
    {
        resp.type = RES_SENSSTATUS;
        resp.payload = msg.payload;
        strcpy(resp.desc, DESC_RES_SENSSTATUS);
    }

    send_msg(current_socket, &resp);
    return CONTINUE_RUNNING;
}

ServerCommand handle_client_activity(Client_t *clients, int peer_socket, int *next_client_index,
                                     fd_set *read_fds, Server type)
{
    for (int i = 0; i < *next_client_index; i++)
    {
        int current_socket = clients[i].socket_id;

        if (FD_ISSET(current_socket, read_fds))
        {
            PeerMsg_t disc = {0};
            int count = recv_msg(current_socket, &disc);

            if (count <= 0)
            {
                printf("Client %d connection lost.\n", clients[i].id);
                clients[i].socket_id = 0;
                clients[i].id = 0;
                clients[i].data = 0;

                for (int j = i; j < *next_client_index - 1; j++)
                {
                    clients[j] = clients[j + 1];
                }
                (*next_client_index)--;

                return CONTINUE_RUNNING;
            }

            for (int j = 0; j < *next_client_index; j++)
            {
                if (disc.payload == clients[j].id)
                {
                    if (disc.type == REQ_DISCSEN)
                    {
                        return handle_req_discsen(current_socket, clients, next_client_index, j, type);
                    }

                    if (disc.type == REQ_SENSSTATUS)
                    {
                        return handle_req_sensstatus(current_socket, peer_socket, clients[j], next_client_index, type);
                    }
                }
            }

            PeerMsg_t err = {0};
            err.type = ERROR_MSG;
            err.payload = 10;
            strcpy(err.desc, "ERROR(10): Sensor not found");
            send_msg(current_socket, &err);

            return CONTINUE_RUNNING;
        }
    }

    return CONTINUE_RUNNING;
}

/**
 * Waits for activity on monitored sockets and handles events accordingly.
 * * @param read_fds File descriptor set to be populated
 * @param max_fd Maximum file descriptor value (not used internally, kept for compatibility)
 * @param server_socket Socket for peer-to-peer communication
 * @param clients_socket Socket for accepting new client connections
 * @param my_peer_id ID of this server in the peer-to-peer communication
 * @param connected_peer_id Pointer to the ID of the connected peer
 * @param client_sockets_ids Array of client socket descriptors
 * @param next_client_index Pointer to the index for the next client socket
 * @param type Type of this server (LOC or STATUS)
 * * @return ServerCommand indicating what action to take next
 */
ServerCommand wait_for_activity(fd_set *read_fds, int server_socket, int clients_socket, int listen_socket,
                                int my_peer_id, int *connected_peer_id, Client_t *clients,
                                int *next_client_index, Server type)
{
    char buf[BUFSZ];

    FD_ZERO(read_fds);
    FD_SET(server_socket, read_fds);
    FD_SET(clients_socket, read_fds);
    FD_SET(listen_socket, read_fds);
    FD_SET(STDIN_FILENO, read_fds);

    int current_max_fd = ((clients_socket > server_socket) ? clients_socket : server_socket);
    current_max_fd = ((current_max_fd > STDIN_FILENO) ? current_max_fd : STDIN_FILENO) + 1;

    for (int i = 0; i < *next_client_index; i++)
    {
        if (clients[i].socket_id > 0)
        {
            FD_SET(clients[i].socket_id, read_fds);
            if (clients[i].socket_id >= current_max_fd)
            {
                current_max_fd = clients[i].socket_id + 1;
            }
        }
    }

    int rv = select(current_max_fd, read_fds, NULL, NULL, NULL);
    if (rv == -1)
    {
        logexit("select");
    }

    if (FD_ISSET(STDIN_FILENO, read_fds))
    {
        return handle_stdin_input(buf, server_socket, my_peer_id);
    }

    if (FD_ISSET(server_socket, read_fds))
    {
        return handle_peer_activity(server_socket, connected_peer_id, clients);
    }

    if (FD_ISSET(listen_socket, read_fds))
    {
        handle_peer_accept(listen_socket, connected_peer_id, &server_socket);
    }

    if (FD_ISSET(clients_socket, read_fds))
    {
        return handle_client_connection(clients_socket, clients, next_client_index,
                                        read_fds, type);
    }

    return handle_client_activity(clients, server_socket, next_client_index, read_fds, type);
}

void manage_peer_connection(int peer_socket, int clients_socket, int listen_socket, int my_peer_id, int *connected_peer_id, Server my_type)
{
    fd_set read_fds;
    ServerCommand status = CONTINUE_RUNNING;
    Client_t clients[MAX_CLIENTS] = {0};
    int next_client_index = 0;

    while (status)
    {
        status = wait_for_activity(&read_fds, peer_socket, clients_socket, listen_socket, my_peer_id, connected_peer_id, clients, &next_client_index, my_type);

        if (status == SERVER_SHUTDOWN)
        {
            if (listen_socket > 0)
            {
                close(listen_socket);
            }
            close(peer_socket);
            close(clients_socket);
            sleep(1);
            exit(EXIT_SUCCESS);
        }

        if (status == TERMINATE_P2P_CONNECTION)
        {
            close(peer_socket);
            close(clients_socket);
            sleep(1);
            break;
        }
    }
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
    Server my_type;
    int connected_peer_id = -1, my_peer_id = -1;
    int csock = -1;

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

    if (connect(s, p2p_addr, sizeof(p2p_storage)) == 0)
    {
        my_type = STATUS;
        my_peer_id = start_active_socket(s, &connected_peer_id);
        csock = init_clients_socket(&clients_storage);

        manage_peer_connection(s, csock, -1, my_peer_id, &connected_peer_id, my_type);
    }
    else
    {
        close(s);
    }

    my_type = LOC;

    int listen_s = socket(p2p_storage.ss_family, SOCK_STREAM, 0);
    if (listen_s == -1)
    {
        logexit("socket");
    }

    if (0 != setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    init_passive_server(listen_s, p2p_addr, p2p_storage);

    int server_socket = -1;

    while (1)
    {
        printf("No peer found, starting to listen...\n");
        my_peer_id = handle_peer_accept(listen_s, &connected_peer_id, &server_socket);

        csock = init_clients_socket(&clients_storage);

        manage_peer_connection(server_socket, csock, listen_s, my_peer_id, &connected_peer_id, my_type);
    }

    return 0;
}