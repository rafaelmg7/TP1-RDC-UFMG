#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024
#define DEFAULT_ID 200
#define ID_FILENAME "client_ids.txt"

void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <server port> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51511 51512\n", argv[0]);
    exit(EXIT_FAILURE);
}

int get_client_id()
{
    int client_id = DEFAULT_ID;
    FILE *fp = fopen(ID_FILENAME, "r+");

    if (fp == NULL)
    {
        fp = fopen(ID_FILENAME, "w+");
        if (fp == NULL)
        {
            logexit("fopen");
        }
    }

    char buf[BUFSZ];
    if (fgets(buf, BUFSZ, fp) != NULL)
    {
        client_id = atoi(buf);
    }

    rewind(fp);
    fprintf(fp, "%d\n", client_id + 1);
    fclose(fp);

    return client_id;
}

int handle_kill(int s, int s_2, int client_id)
{
    PeerMsg_t disc = {0};
    disc.type = REQ_DISCSEN;
    disc.payload = client_id;

    if (send_msg(s, &disc) == -1 || send_msg(s_2, &disc) == -1)
    {
        printf("Error sending disconnect request\n");
        close(s);
        close(s_2);
        return -1;
    }

    PeerMsg_t resp1 = {0}, resp2 = {0};
    int recv1 = recv_msg(s, &resp1);
    int recv2 = recv_msg(s_2, &resp2);

    close(s);
    close(s_2);

    if (recv1 == -1 || recv2 == -1)
    {
        printf("Error receiving disconnect response\n");
        return -1;
    }

    printf("%s\n", resp1.desc);
    printf("%s\n", resp2.desc);
    return 0;
}

int handle_check_failure(int ss_socket, int client_id)
{
    PeerMsg_t disc = {0};
    disc.type = REQ_SENSSTATUS;
    disc.payload = client_id;
    strcpy(disc.desc, DESC_REQ_SENSSTATUS);

    if (send_msg(ss_socket, &disc) == -1)
    {
        printf("Error sending check failure request\n");
        close(ss_socket);
        return -1;
    }

    PeerMsg_t resp = {0};

    if (recv_msg(ss_socket, &resp) <= 0)
    {
        printf("Error receiving check failure response\n");
        return -1;
    }

    if(resp.type == RES_SENSSTATUS){
        int location = resp.payload;
        char area[BUFSZ];
        
        if(location >= 1 && location <= 3) {
            strcpy(area, "1 (Norte)");
        } else if(location == 4 || location == 5) {
            strcpy(area, "2 (Sul)");
        } else if(location == 6 || location == 7) {
            strcpy(area, "3 (Leste)");
        } else if(location >= 8 && location <= 10) {
            strcpy(area, "4 (Oeste)");
        } else {
            strcpy(area, "Unknown");
        }
        
        printf("Alert received from area: %s\n", area);
    }

    return 0;
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
    int ss_socket, sl_socket;
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
        logexit("connect");
    }

    if (connect(s_2, addr_2, sizeof(storage_2)) != 0)
    {
        logexit("connect");
    }

    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);

    PeerMsg_t msg1 = {0}, msg2 = {0};
    int client_id = get_client_id();

    msg1.type = msg2.type = REQ_CONNSEN;
    msg1.payload = msg2.payload = client_id;
    send_msg(s, &msg1);
    send_msg(s_2, &msg2);

    memset(&msg1, 0, sizeof(msg1));

    recv_msg(s, &msg1);
    recv_msg(s_2, &msg2);

    if (msg1.type == ERROR_MSG)
    {
        printf("%s\n", msg1.desc);
        close(s);
        logexit(msg1.desc);
    }

    if (msg2.type == ERROR_MSG)
    {
        printf("%s\n", msg2.desc);
        close(s_2);
        logexit(msg2.desc);
    }

    if (msg1.type != RES_CONNSEN || msg2.type != RES_CONNSEN)
    {
        close(s);
        close(s_2);
        logexit("Unexpected message type");
    }

    if(strncmp(msg1.desc, "SS", 2) == 0){
        ss_socket = s;
        sl_socket = s_2;
    } else {
        ss_socket = s_2;
        sl_socket = s;
    }

    printf("%s New ID: %d\n", msg1.desc, msg1.payload);
    printf("%s New ID: %d\n", msg2.desc, msg2.payload);
    printf("Ok(02)\n");

    unsigned total = 0;
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            toLowerString(buf);
            if (strncmp(buf, "kill", 4) == 0)
            {
                int result = handle_kill(s, s_2, client_id);
                if (result == -1)
                {
                    logexit("Error handling kill command");
                }

                break;
            }

            if(strncmp(buf, "check failure", 13) == 0)
            {
                int result = handle_check_failure(ss_socket, client_id);
                if (result == -1)
                {
                    logexit("Error handling check failure command");
                }
                continue;
            }
        }
    }
    close(s);
    close(s_2);

    printf("received %u bytes\n", total);
    puts(buf);

    exit(EXIT_SUCCESS);
}