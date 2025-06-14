#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>

/**
 * @brief Exibe a forma correta de usar o programa e o encerra.
 * @param argc O número de argumentos da linha de comando.
 * @param argv O array de strings dos argumentos.
 */
void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <p2p server port> <clients server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51500 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

/**
 * @brief Gera um ID único para um peer.
 * * Esta função utiliza uma variável estática para manter o controle do último ID gerado,
 * garantindo que cada chamada retorne um novo ID. Ela também evita a colisão com
 * o ID do outro peer (`other_id`).
 * * @param other_id O ID do peer conectado para evitar duplicação.
 * @return int O novo ID gerado para o peer.
 */
int get_peer_id(int other_id)
{
    static int id = 100;
    if (id != other_id)
    {
        return id++;
    }

    return ++id;
}

/**
 * @brief Gera um número aleatório para representar a localização de um cliente.
 * @return int A localização gerada (entre 1 e 10).
 */
int get_client_loc()
{
    int random = rand() % 10 + 1;
    return random;
}

/**
 * @brief Gera um número aleatório para representar o status de um cliente.
 * @return int O status gerado (0 ou 1).
 */
int get_client_status()
{
    int random = rand() % 2;
    return random;
}

/**
 * @brief Configura o socket para atuar como um servidor passivo (de escuta).
 * * Realiza o bind do socket a um endereço e porta específicos e o coloca
 * em modo de escuta para aguardar conexões de outros peers.
 * * @param s O file descriptor do socket de escuta.
 * @param p2p_addr A estrutura de endereço para o bind.
 * @param p2p_storage O armazenamento do endereço.
 */
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

/**
 * @brief Aceita uma nova conexão de peer e realiza o handshake inicial.
 * * Aguarda e aceita uma conexão em um socket de escuta, troca mensagens
 * com o novo peer para estabelecer os IDs de cada um e retorna o ID do peer conectado.
 * * @param s O socket de escuta.
 * @param connected_peer_id Ponteiro para armazenar o ID do peer que se conectou.
 * @param server_sock Ponteiro para armazenar o file descriptor do novo socket de comunicação.
 * @return int O ID do peer recém-conectado.
 */
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

    Msg_t msg = {0};
    recv_msg(s_sock, &msg);
    if (msg.type == REQ_CONPEER)
    {
        if (*connected_peer_id != -1)
        {
            Msg_t err = {0};
            err.type = ERROR_MSG;
            err.payload = PEER_LIMIT_ERROR;
            strcpy(err.desc, "Peer limit exceeded");
            send_msg(s_sock, &err);
            close(s_sock);
            return 0;
        }

        *connected_peer_id = get_peer_id(0);
        Msg_t resp = {0};
        resp.type = RES_CONPEER;
        resp.payload = *connected_peer_id;
        send_msg(s_sock, &resp);
        printf("Peer %d connected\n", *connected_peer_id);

        Msg_t peer_id_msg = {0};
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

/**
 * @brief Inicia uma conexão ativa com outro peer e realiza o handshake.
 * * Envia uma requisição de conexão para um peer, recebe a resposta e
 * estabelece os IDs de comunicação.
 * * @param s O socket para se conectar.
 * @param connected_peer_id Ponteiro para armazenar o ID do peer ao qual se conectou.
 * @return int O ID deste servidor, recebido do peer.
 */
int start_active_socket(int s, int *connected_peer_id)
{
    int my_peer_id;

    Msg_t msg = {0};
    msg.type = REQ_CONPEER;
    send_msg(s, &msg);
    recv_msg(s, &msg);

    if (msg.type == ERROR_MSG)
    {
        close(s);
        logexit(msg.desc);
    }

    if (msg.type == RES_CONPEER)
    {
        printf("New Peer ID: %d\n", msg.payload);
        my_peer_id = msg.payload;

        *connected_peer_id = get_peer_id(my_peer_id);
        Msg_t resp = {0};
        resp.type = RES_CONPEER;
        resp.payload = *connected_peer_id;
        send_msg(s, &resp);
        printf("Peer %d connected\n", *connected_peer_id);
    }

    return my_peer_id;
}

/**
 * @brief Inicializa o socket para aceitar conexões de clientes.
 * * Cria um socket, o associa a um endereço/porta e o prepara para
 * escutar conexões de clientes (sensores).
 * * @param clients_storage A estrutura de armazenamento de endereço para clientes.
 * @return int O file descriptor do socket de escuta de clientes.
 */
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

/**
 * @brief Trata a requisição CHECKALERT de um peer.
 * * Esta função é chamada quando o servidor de localização recebe uma
 * requisição do servidor de status. Ela envia a localização do sensor
 * solicitado de volta para o peer.
 * * @param peer_socket O socket do peer.
 * @param client O cliente (sensor) cuja localização é solicitada.
 * @param next_client_index (Não utilizado aqui)
 * @param type O tipo de servidor (LOC).
 * @return ServerCommand O estado de continuação do servidor.
 */
ServerCommand handle_server_checkalert(int peer_socket, Client_t client,
                                       int *next_client_index, Server type)
{
    printf("Found location of sensor %d: location %d\n", client.id, client.data);
    printf("Sending RES_CHECKALERT %d to SS\n", client.data);

    Msg_t msg = {0};
    msg.type = RES_CHECKALERT;
    msg.payload = client.data;
    send_msg(peer_socket, &msg);
    return CONTINUE_RUNNING;
}

/**
 * @brief Processa a entrada do usuário via terminal (stdin).
 * * Detecta o comando "kill" para iniciar o processo de desconexão
 * do peer e encerrar o servidor de forma limpa.
 * * @param buf Buffer para ler a entrada.
 * @param server_socket O socket de comunicação com o peer.
 * @param my_peer_id O ID deste servidor.
 * @return ServerCommand Retorna SERVER_SHUTDOWN para encerrar ou CONTINUE_RUNNING.
 */
ServerCommand handle_stdin_input(char *buf, int server_socket, int my_peer_id)
{
    memset(buf, 0, BUFSZ);
    if (fgets(buf, BUFSZ, stdin) != NULL)
    {
        if (strncmp(buf, "kill", 4) == 0)
        {
            Msg_t disc = {0};
            disc.type = REQ_DISCPEER;
            disc.payload = my_peer_id;

            send_msg(server_socket, &disc);

            memset(&disc, 0, sizeof(disc));
            recv_msg(server_socket, &disc);

            if (disc.type == OK_MSG)
            {
                printf("%s\n", disc.desc);
                printf("Peer %d disconnected\n", disc.payload);
            }
            else if (disc.type == ERROR_MSG)
            {
                logexit(disc.desc);
            }

            return SERVER_SHUTDOWN;
        }
    }
    return CONTINUE_RUNNING;
}

/**
 * @brief Gerencia a comunicação e as mensagens recebidas do peer conectado.
 * * Processa diferentes tipos de mensagens do peer, como desconexão (`REQ_DISCPEER`)
 * e verificação de alerta (`REQ_CHECKALERT`).
 * * @param server_socket O socket de comunicação com o peer.
 * @param connected_peer_id Ponteiro para o ID do peer conectado.
 * @param clients Array de clientes (sensores) para consulta.
 * @return ServerCommand O estado de continuação do servidor.
 */
ServerCommand handle_peer_activity(int server_socket, int *connected_peer_id, Client_t *clients)
{
    Msg_t disc = {0};
    size_t count = recv_msg(server_socket, &disc);
    if (count == 0)
    {
        printf("Peer %d disconnected\n", *connected_peer_id);
        *connected_peer_id = -1;
        return TERMINATE_P2P_CONNECTION;
    }

    if (disc.type == REQ_DISCPEER)
    {
        if (disc.payload != *connected_peer_id)
        {
            Msg_t err = {0};
            err.type = ERROR_MSG;
            err.payload = 2;
            strcpy(err.desc, DESC_ERROR_02);
            send_msg(server_socket, &err);
            return CONTINUE_RUNNING;
        }

        Msg_t ok = {0};
        ok.type = OK_MSG;
        ok.payload = *connected_peer_id;
        strcpy(ok.desc, DESC_OK_01);
        send_msg(server_socket, &ok);
        printf("Peer %d disconnected\n", *connected_peer_id);

        *connected_peer_id = -1;
        return TERMINATE_P2P_CONNECTION;
    }

    if (disc.type == REQ_CHECKALERT)
    {
        printf("REQ_CHECKALERT %d\n", disc.payload);
        Client_t client = {0};
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (disc.payload == clients[i].id)
            {
                client = clients[i];
                return handle_server_checkalert(server_socket, client, NULL, LOC);
            }
        }

        Msg_t err = {0};
        err.type = ERROR_MSG;
        err.payload = 10;
        strcpy(err.desc, DESC_ERROR_10);
        printf("ERROR(10) - Sensor not found\n");
        send_msg(server_socket, &err);

        return CONTINUE_RUNNING;
    }

    return CONTINUE_RUNNING;
}

/**
 * @brief Aceita e gerencia uma nova conexão de cliente (sensor).
 * * Adiciona o novo cliente à lista de clientes conectados e atribui a ele
 * um dado (localização ou status) dependendo do tipo de servidor.
 * * @param clients_socket O socket de escuta para clientes.
 * @param clients Array de clientes conectados.
 * @param next_client_index Ponteiro para o índice do próximo slot livre no array de clientes.
 * @param read_fds (Não utilizado diretamente aqui)
 * @param type O tipo do servidor (LOC ou STATUS).
 * @return ServerCommand O estado de continuação do servidor.
 */
ServerCommand handle_client_connection(int clients_socket, Client_t *clients,
                                       int *next_client_index, fd_set *read_fds, Server type)
{
    struct sockaddr_storage cstorage;
    socklen_t caddrlen = sizeof(cstorage);
    Msg_t msg = {0};

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
            Msg_t err = {0};
            err.type = ERROR_MSG;
            err.payload = CLIENT_LIMIT_ERROR;
            strcpy(err.desc, DESC_ERROR_09);
            send_msg(csock, &err);
            close(csock);
            return CONTINUE_RUNNING;
        }

        Msg_t resp = {0};
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

/**
 * @brief Processa a solicitação de desconexão (`REQ_DISCSEN`) de um cliente.
 * * Remove o cliente da lista de clientes ativos e reorganiza a lista.
 * * @param current_socket O socket do cliente que pediu para desconectar.
 * @param clients O array de clientes.
 * @param next_client_index Ponteiro para o número de clientes.
 * @param client_index O índice do cliente a ser removido.
 * @param type O tipo do servidor.
 * @return ServerCommand O estado de continuação do servidor.
 */
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

    Msg_t ok = {0};
    ok.type = OK_MSG;
    ok.payload = 1;
    sprintf(ok.desc, "%s Successful disconnect", type == LOC ? "SL" : "SS");
    send_msg(current_socket, &ok);
    printf("Client %d removed\n", clients[client_index].id);

    return CONTINUE_RUNNING;
}

/**
 * @brief Processa uma solicitação de status (`REQ_SENSSTATUS`) de um cliente.
 * * Se o status do sensor indicar uma falha (status 1), o servidor consulta o
 * peer (servidor de localização) para obter a localização do sensor e a envia
 * ao cliente. Caso contrário, envia uma mensagem de OK.
 * * @param current_socket O socket do cliente solicitante.
 * @param peer_socket O socket do peer (servidor de localização).
 * @param client O cliente (sensor) que fez a solicitação.
 * @return ServerCommand O estado de continuação do servidor.
 */
ServerCommand handle_req_sensstatus(int current_socket, int peer_socket, Client_t client)
{
    Msg_t msg = {0};

    if (client.data != 1)
    {
        msg.type = OK_MSG;
        msg.payload = 2;
        strcpy(msg.desc, DESC_OK_02);
        send_msg(current_socket, &msg);
        return CONTINUE_RUNNING;
    }

    printf("Sensor %d status = 1 (failure detected)\n", client.id);
    printf("Sending REQ_CHECKALERT %d to SL\n", client.id);

    msg.type = REQ_CHECKALERT;
    msg.payload = client.id;
    send_msg(peer_socket, &msg);

    memset(&msg, 0, sizeof(msg));

    recv_msg(peer_socket, &msg);

    Msg_t resp = {0};

    if (msg.type == ERROR_MSG)
    {
        printf("ERROR(%d) received from SL\n", msg.payload);
        printf("Sending ERROR(%d) to CLIENT\n", msg.payload);
        resp.type = ERROR_MSG;
        resp.payload = 10;
        strcpy(resp.desc, DESC_ERROR_10);
    }

    if (msg.type == RES_CHECKALERT)
    {
        printf("RES_CHECKALERT %d\n", msg.payload);
        printf("Sending RES_SENSSTATUS %d to CLIENT\n", msg.payload);

        resp.type = RES_SENSSTATUS;
        resp.payload = msg.payload;
    }

    send_msg(current_socket, &resp);
    return CONTINUE_RUNNING;
}

/**
 * @brief Processa uma solicitação de localização (`REQ_SENSLOC`) de um cliente.
 * * Envia a localização armazenada do sensor de volta para o cliente.
 * * @param current_socket O socket do cliente solicitante.
 * @param client O cliente (sensor) que fez a solicitação.
 * @return ServerCommand O estado de continuação do servidor.
 */
ServerCommand handle_req_sensloc(int current_socket, Client_t client)
{
    Msg_t msg = {0};

    if (client.data < 1)
    {
        msg.type = ERROR_MSG;
        msg.payload = 10;
        strcpy(msg.desc, DESC_ERROR_10);
        send_msg(current_socket, &msg);
        return CONTINUE_RUNNING;
    }

    msg.type = RES_SENSLOC;
    msg.payload = client.data;
    send_msg(current_socket, &msg);
    return CONTINUE_RUNNING;
}

/**
 * @brief Processa uma solicitação de lista de sensores por localização (`REQ_LOCLIST`).
 * * Busca todos os sensores na localização especificada e retorna uma lista
 * com seus IDs para o cliente.
 * * @param current_socket O socket do cliente solicitante.
 * @param loc_id O ID da localização a ser buscada.
 * @param clients O array de clientes para buscar.
 * @return ServerCommand O estado de continuação do servidor.
 */
ServerCommand handle_req_loclist(int current_socket, int loc_id, Client_t *clients)
{
    char loc_clients[BUFSZ];
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].data == loc_id)
        {
            count++;
            char client_id[10];
            if (count > 1)
            {
                sprintf(client_id, ", %d", clients[i].id);
            }
            else
            {
                memset(loc_clients, 0, BUFSZ);
                sprintf(loc_clients, "%d", clients[i].id);
            }
            strcat(loc_clients, client_id);
        }
    }

    if (loc_id < 1 || loc_id > 10 || count == 0)
    {
        Msg_t err = {0};
        err.type = ERROR_MSG;
        err.payload = 11;
        printf("Location %d not found\n", loc_id);
        printf("Sending ERROR(11) to CLIENT\n");
        strcpy(err.desc, DESC_ERROR_11);
        send_msg(current_socket, &err);
        return CONTINUE_RUNNING;
    }

    printf("Found sensors at location %d\n", loc_id);
    printf("Sending RES_LOCLIST %s", loc_clients);

    Msg_t msg = {0};
    msg.type = RES_LOCLIST;
    msg.payload = loc_id;
    strcpy(msg.desc, loc_clients);
    send_msg(current_socket, &msg);

    return CONTINUE_RUNNING;
}

/**
 * @brief Gerencia a comunicação e as mensagens recebidas dos clientes.
 * * Verifica qual cliente enviou dados e direciona a mensagem para a
 * função de tratamento apropriada (desconexão, status, localização, etc.).
 * * @param clients O array de clientes conectados.
 * @param peer_socket O socket do peer (necessário para `handle_req_sensstatus`).
 * @param next_client_index Ponteiro para o número de clientes.
 * @param read_fds O conjunto de file descriptors prontos para leitura.
 * @param type O tipo do servidor.
 * @return ServerCommand O estado de continuação do servidor.
 */
ServerCommand handle_client_activity(Client_t *clients, int peer_socket, int *next_client_index,
                                     fd_set *read_fds, Server type)
{
    for (int i = 0; i < *next_client_index; i++)
    {
        int current_socket = clients[i].socket_id;

        if (FD_ISSET(current_socket, read_fds))
        {
            Msg_t disc = {0};
            size_t count = recv_msg(current_socket, &disc);

            if (count == 0)
            {
                printf("Client %d removed\n", clients[i].id);
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

            if (disc.type == REQ_LOCLIST)
            {
                printf("REQ_LOCLIST %d\n", disc.payload);
                return handle_req_loclist(current_socket, disc.payload, clients);
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
                        printf("REQ_SENSSTATUS %d\n", clients[j].id);
                        return handle_req_sensstatus(current_socket, peer_socket, clients[j]);
                    }

                    if (disc.type == REQ_SENSLOC)
                    {
                        printf("REQ_SENSLOC %d\n", clients[j].id);
                        return handle_req_sensloc(current_socket, clients[j]);
                    }
                }
            }

            Msg_t err = {0};
            err.type = ERROR_MSG;
            err.payload = 10;
            strcpy(err.desc, DESC_ERROR_10);
            send_msg(current_socket, &err);

            return CONTINUE_RUNNING;
        }
    }

    return CONTINUE_RUNNING;
}

/**
 * @brief Aguarda por atividade em múltiplos sockets usando `select`.
 * * Monitora a entrada padrão, o socket do peer, o socket de escuta de clientes
 * e todos os sockets de clientes conectados. Quando uma atividade é detectada,
 * chama a função de tratamento correspondente.
 * * @param read_fds O conjunto de file descriptors a ser monitorado.
 * @param server_socket O socket de comunicação com o peer.
 * @param clients_socket O socket de escuta para clientes.
 * @param listen_socket O socket de escuta para peers.
 * @param my_peer_id O ID deste servidor.
 * @param connected_peer_id Ponteiro para o ID do peer conectado.
 * @param clients Array de clientes.
 * @param next_client_index Ponteiro para o número de clientes.
 * @param type O tipo do servidor.
 * @return ServerCommand O comando resultante da atividade.
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

/**
 * @brief Loop principal que gerencia a conexão com o peer e com os clientes.
 * * Chama `wait_for_activity` repetidamente para processar todos os eventos
 * de rede e do usuário, mantendo o servidor em execução até que a conexão
 * com o peer seja encerrada ou o servidor seja desligado.
 * * @param peer_socket O socket de comunicação com o peer.
 * @param clients_socket O socket de escuta de clientes.
 * @param listen_socket O socket de escuta de peers.
 * @param my_peer_id O ID deste servidor.
 * @param connected_peer_id Ponteiro para o ID do peer conectado.
 * @param my_type O tipo deste servidor.
 */
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

/**
 * @brief Função principal do programa.
 * * A lógica principal é:
 * 1. Tenta se conectar a um peer existente. Se conseguir, assume o papel de
 * servidor de STATUS (ativo).
 * 2. Se a conexão falhar, assume que não há outro peer e começa a escutar por
 * conexões, assumindo o papel de servidor de LOCALIZAÇÃO (passivo).
 * 3. Após estabelecer uma conexão (seja ativa ou passiva), entra em um loop
 * para gerenciar a comunicação com o peer e os clientes.
 */
int main(int argc, char **argv)
{
    // Verifica se os argumentos da linha de comando estão corretos
    if (argc < 4)
    {
        usage(argc, argv);
    }

    // Inicializa o gerador de números aleatórios
    srand(time(NULL));

    // Estruturas para armazenar endereços de sockets
    struct sockaddr_storage clients_storage;
    struct sockaddr_storage p2p_storage;
    Server my_type;
    int connected_peer_id = -1, my_peer_id = -1;
    int csock = -1; // Socket para clientes

    // Inicializa as estruturas de endereço a partir dos argumentos
    if (0 != server_sockaddr_init(argv[1], argv[2], argv[3], &p2p_storage, &clients_storage))
    {
        usage(argc, argv);
    }

    // Cria o socket para a comunicação P2P
    int s = socket(p2p_storage.ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        logexit("socket");
    }

    // Permite a reutilização do endereço do socket
    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    struct sockaddr *p2p_addr = (struct sockaddr *)(&p2p_storage);

    // --- MODO ATIVO (Servidor de Status) ---
    // Tenta se conectar a um servidor peer existente
    if (connect(s, p2p_addr, sizeof(p2p_storage)) == 0)
    {
        // Conexão bem-sucedida, assume o papel de Servidor de Status (SS)
        my_type = STATUS;
        my_peer_id = start_active_socket(s, &connected_peer_id);
        csock = init_clients_socket(&clients_storage);

        // Entra no loop principal para gerenciar a conexão
        manage_peer_connection(s, csock, -1, my_peer_id, &connected_peer_id, my_type);
    }
    else
    {
        // `connect` falhou, então não há um peer para se conectar. Fecha o socket.
        close(s);
    }

    // --- MODO PASSIVO (Servidor de Localização) ---
    // Se o código chegou aqui, a tentativa de conexão ativa falhou.
    // O servidor agora se tornará passivo (Servidor de Localização - SL) e aguardará uma conexão.
    my_type = LOC;

    // Cria um novo socket para escutar por conexões de peers
    int listen_s = socket(p2p_storage.ss_family, SOCK_STREAM, 0);
    if (listen_s == -1)
    {
        logexit("socket");
    }

    if (0 != setsockopt(listen_s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    // Configura o socket de escuta
    init_passive_server(listen_s, p2p_addr, p2p_storage);

    int server_socket = -1;

    // Loop infinito para sempre voltar a escutar após uma desconexão de peer
    while (1)
    {
        printf("No peer found, starting to listen...\n");
        // Aguarda e aceita uma conexão de um novo peer
        my_peer_id = handle_peer_accept(listen_s, &connected_peer_id, &server_socket);

        // Inicializa o socket para escutar conexões de clientes
        csock = init_clients_socket(&clients_storage);

        // Entra no loop para gerenciar a conexão com o peer e os clientes
        manage_peer_connection(server_socket, csock, listen_s, my_peer_id, &connected_peer_id, my_type);
    }

    return 0;
}