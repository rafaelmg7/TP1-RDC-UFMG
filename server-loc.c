#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <p2p server port> <clients server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51500 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

void send_response(int socket, char *buf, int buf_size)
{
    size_t count = send(socket, buf, strlen(buf) + 1, 0);
    if (count != strlen(buf) + 1)
    {
        logexit("send");
    }
    memset(buf, 0, buf_size);
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        usage(argc, argv);
    }

    struct sockaddr_storage clients_storage;
    struct sockaddr_storage p2p_storage;
    Server_Type my_type;
    int peerId = -1;
    int clientIds[15] = {-1};
    // fd_set readfds;
    if (0 != server_sockaddr_init(argv[1], argv[2], argv[3], &p2p_storage, &clients_storage))
    {
        usage(argc, argv);
    }

    // prinf("aqui %s", inet_ntoa(&p2p_storage.ss_family));

    int s;
    s = socket(p2p_storage.ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        logexit("socket");
    }

    // printf("passou");

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    struct sockaddr *p2p_addr = (struct sockaddr *)(&p2p_storage);
    // if (0 != bind(s, p2p_addr, sizeof(p2p_storage)))
    // {
    //     logexit("bind");
    // }

    // if (0 != listen(s, 10))
    // {
    //     logexit("listen");
    // }

    // struct sockaddr *addr = (struct sockaddr *)(&storage);

    // FD_ZERO(&readfds);

    // add our descriptors to the set
    // FD_SET(s, &readfds);
    if (0 != connect(s, p2p_addr, sizeof(p2p_storage)))
    {
        // logexit("connect");
        printf("No peer found, starting to listen...\n");
        my_type = LOC;
        int peers[MAX_PEERS] = {-1};
        if (0 != bind(s, p2p_addr, sizeof(p2p_storage)))
        {
            logexit("bind");
        }

        if (0 != listen(s, 10))
        {
            logexit("listen");
        }

        char addrstr[BUFSZ];
        addrtostr(p2p_addr, addrstr, BUFSZ);
        while (1)
        {
            struct sockaddr_storage cstorage;
            struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
            socklen_t caddrlen = sizeof(cstorage);

            int csock = accept(s, caddr, &caddrlen);
            if (csock == -1)
            {
                logexit("accept");
            }
            char buf[BUFSZ];
            memset(buf, 0, BUFSZ);
            if (peers[0] != -1)
            {
                sprintf(buf, "ERROR(01) (completar msgs)\n");
                send_response(csock, buf, BUFSZ);
                continue;
            }

            peerId = csock;
            peers[0] = peerId;

            // char caddrstr[BUFSZ];
            // addrtostr(caddr, caddrstr, BUFSZ);
            // printf("[log] connection from %s\n", caddrstr);

            printf("Peer %d connected\n", peerId);
            sprintf(buf, "21 %d\n", peerId);
            send_response(csock, buf, BUFSZ);

            size_t count = recv(csock, buf, BUFSZ - 1, 0);
            printf("New Peer ID: %s\n", buf);

            // count = recv(csock, buf, BUFSZ - 1, 0);
            // printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, buf);

            memset(buf, 0, BUFSZ);
            // sprintf(buf, "remote endpoint: %.1000s\n", caddrstr);
            // count = send(csock, buf, strlen(buf) + 1, 0);
            // if (count != strlen(buf) + 1)
            // {
            //     logexit("send");
            // }
            close(csock);
        }
        exit(EXIT_SUCCESS);

        // int s2 = socket(p2p_storage.ss_family, SOCK_STREAM, 0);
        // FD_SET(s2, &readfds);

        // struct sockaddr *addr2 = (struct sockaddr *)(&p2p_storage);
        // if (0 != bind(s, addr2, sizeof(p2p_storage)))
        // {
        //     logexit("bind");
        // }

        // if (0 != listen(s, 10))
        // {
        //     logexit("listen");
        // }

        // // select passa a esperar tanto comunicação do outro servidor quanto de algum cliente
        // // temos que esperar por conexão dos clientes com accept? como será isso?
        // char buf[BUFSZ];
        // char buf2[BUFSZ];
        // memset(buf, 0, BUFSZ);
        // memset(buf2, 0, BUFSZ);
        // int rv = select(s2 + 1, &readfds, NULL, NULL, NULL);
        // if (rv == -1)
        // {
        //     perror("select"); // error occurred in select()
        // }
        // else
        // {
        //     // one or both of the descriptors have data
        //     if (FD_ISSET(s, &readfds))
        //     {
        //         recv(s, buf, sizeof(buf), 0); // substituir por funcao que faz algo
        //     }
        //     if (FD_ISSET(s2, &readfds))
        //     {
        //         recv(s2, buf2, sizeof(buf2), 0); // substituir por funcao que faz algo
        //     }
        // }
        // while (1)
        // {
        //     struct sockaddr_storage cstorage2;
        //     struct sockaddr *caddr2 = (struct sockaddr *)(&cstorage2);
        //     socklen_t caddrlen2 = sizeof(cstorage2);

        //     int csock2 = accept(s2, caddr2, &caddrlen2);
        //     if (csock2 == -1)
        //     {
        //         logexit("accept");
        //     }

        //     char caddrstr2[BUFSZ];
        //     addrtostr(caddr2, caddrstr2, BUFSZ);
        //     printf("[log] connection from %s\n", caddrstr2);
        // }
    }

    my_type = STATUS;

    // char new_addrstr[BUFSZ];
    // addrtostr(p2p_addr, new_addrstr, BUFSZ);

    // printf("connected to %s\n", new_addrstr);

    char rec_buf[BUFSZ];
    memset(rec_buf, 0, BUFSZ);
    unsigned total = 0;
    size_t count;
    // while (1)
    // {
    printf("esperando\n");
    count = recv(s, rec_buf + total, BUFSZ - total, 0);
    // puts(rec_buf);
    // if (count == 0)
    // {
    //     // Connection terminated.
    //     printf("saiu");
    //     break;
    // }
    total += count;
    // }

    printf("New Peer ID: %s\n", rec_buf);
    peerId = (int)rec_buf[3] + 1;
    memset(rec_buf, 0, BUFSZ);
    sprintf(rec_buf, "21 %d\n", peerId);
    send_response(s, rec_buf, BUFSZ);
    char buf[BUFSZ];
    total = 0;
    while (strcmp(buf, "kill") != 0)
    {
        // memset(buf, 0, BUFSZ);
        memset(rec_buf, 0, BUFSZ);
        printf("mensagem> ");
        fgets(buf, BUFSZ - 1, stdin);
        // size_t count = send(s, buf, strlen(buf) + 1, 0);
        // if (count != strlen(buf) + 1)
        // {
        //     logexit("send");
        // }

        // while (1)
        // {
        //     count = recv(s, rec_buf + total, BUFSZ - total, 0);
        //     if (count == 0)
        //     {
        //         // Connection terminated.
        //         break;
        //     }
        //     total += count;
        // }
    }
    close(s);

    printf("received %u bytes\n", total);
    puts(rec_buf);

    exit(EXIT_SUCCESS);
}