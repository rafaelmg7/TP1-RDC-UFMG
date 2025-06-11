#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <server port> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51511 51512\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    struct sockaddr_storage storage_2;
    if (0 != client_sockaddr_init(argv[1], argv[2], argv[3], &storage, &storage_2))
    {
        usage(argc, argv);
    }

    int s, s_2;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    s_2 = socket(storage_2.ss_family, SOCK_STREAM, 0);
    if (s == -1 || s_2 == -1)
    {
        logexit("socket");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    struct sockaddr *addr_2 = (struct sockaddr *)(&storage_2);
    if (connect(s, addr, sizeof(storage)) != 0)
    {
        printf("a");
        logexit("connect");
    }

    if (connect(s_2, addr_2, sizeof(storage_2)) != 0)
    {
        printf("b");
        logexit("connect");
    }

    // char addrstr[BUFSZ];
    // addrtostr(addr, addrstr, BUFSZ);

    // printf("connected to %s\n", addrstr);

    char buf[BUFSZ];
    // memset(buf, 0, BUFSZ);
    // printf("mensagem> ");
    // fgets(buf, BUFSZ - 1, stdin);
    // size_t count = send(s, buf, strlen(buf) + 1, 0);
    // if (count != strlen(buf) + 1)
    // {
    //     logexit("send");
    // }

    PeerMsg_t msg = {0};
    // int id_cliente = gerar_id_cliente();
    memset(buf, 0, BUFSZ);
    printf("id> ");
    fgets(buf, BUFSZ - 1, stdin);
    int id_cliente = atoi(buf);
    if (id_cliente <= 0)
    {
        logexit("Invalid client ID");
    }
    msg.type = REQ_CONNSEN;
    msg.payload = id_cliente;
    send_msg(s, &msg);
    send_msg(s_2, &msg);

    memset(&msg, 0, sizeof(msg));

    recv_msg(s, &msg);
    if (msg.type == RES_CONNSEN)
    {
        printf("SS New ID: %d\n", msg.payload);
        printf("SL New ID: %d\n", id_cliente);
        printf("Ok(02)\n");
    }
    else if (msg.type == ERROR_MSG)
    {
        printf("%s\n", msg.desc);
        close(s);
        logexit(msg.desc);
    }

    // memset(buf, 0, BUFSZ);
    unsigned total = 0;
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            if (strncmp(buf, "kill", 4) == 0)
            {
                PeerMsg_t disc = {0};
                PeerMsg_t disc2 = {0};
                disc.type = REQ_DISCPEER;
                disc.payload = id_cliente;
                send_msg(s, &disc);
                send_msg(s_2, &disc);

                memset(&disc, 0, sizeof(disc));
                memset(&disc2, 0, sizeof(disc2));
                recv_msg(s, &disc);
                recv_msg(s_2, &disc2);
                if (disc.type == OK_MSG && disc2.type == OK_MSG)
                {
                    printf("%s\n", disc.desc);
                }
                else if (disc.type == ERROR_MSG || disc2.type == ERROR_MSG)
                {
                    printf("%s\n", disc.desc);
                    logexit(disc.desc);
                }
                else
                {
                    logexit("Unknown error occurred during disconnect.");
                }

                // return 1;
                break;
            }
        }
    }
    close(s);

    printf("received %u bytes\n", total);
    puts(buf);

    exit(EXIT_SUCCESS);
}