#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define DEFAULT_ID 200
#define ID_FILENAME "client_ids.txt"

/**
 * @brief Exibe a forma correta de usar o programa e o encerra.
 * @param argc O número de argumentos da linha de comando.
 * @param argv O array de strings dos argumentos.
 */
void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <server port> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51511 51512\n", argv[0]);
    exit(EXIT_FAILURE);
}

/**
 * @brief Obtém um ID de cliente único a partir de um arquivo de persistência.
 *
 * Esta função lê um número de um arquivo (`client_ids.txt`), que representa o ID a ser usado.
 * Em seguida, incrementa esse número e o salva de volta no arquivo para garantir que
 * a próxima instância do cliente receba um ID diferente. Se o arquivo não existir,
 * ele é criado com um valor padrão.
 *
 * @return int O ID único para esta instância do cliente.
 */
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

/**
 * @brief Trata atividade genérica vinda de um dos servidores.
 *
 * A função é chamada quando `select()` detecta dados em um dos sockets do servidor.
 * Ela tenta receber uma mensagem para verificar se a conexão ainda está ativa.
 *
 * @param s O socket do servidor que apresentou atividade.
 * @return int Retorna 1 se a conexão estiver ativa, 0 se a conexão for encerrada.
 */
int handle_server_activity(int s)
{
    Msg_t msg = {0};
    int count = recv_msg(s, &msg);

    if (count <= 0)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief Processa o comando 'kill' para encerrar o cliente.
 *
 * Envia uma requisição de desconexão (`REQ_DISCSEN`) para ambos os servidores (SL e SS),
 * aguarda as respostas e encerra as conexões de forma limpa.
 *
 * @param s O socket do primeiro servidor.
 * @param s_2 O socket do segundo servidor.
 * @param client_id O ID deste cliente.
 * @return int Retorna 0 em sucesso, -1 em caso de falha.
 */
int handle_kill(int s, int s_2, int client_id)
{
    Msg_t disc = {0};
    disc.type = REQ_DISCSEN;
    disc.payload = client_id;

    if (send_msg(s, &disc) == -1 || send_msg(s_2, &disc) == -1)
    {
        printf("Error sending disconnect request\n");
        close(s);
        close(s_2);
        return -1;
    }

    Msg_t resp1 = {0}, resp2 = {0};
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

/**
 * @brief Processa o comando 'check failure'.
 *
 * Envia uma requisição (`REQ_SENSSTATUS`) para o Servidor de Status (SS) para verificar
 * a condição de falha do sensor. Imprime a resposta recebida, que pode ser um OK,
 * um erro, ou, em caso de falha, a área geográfica do alerta.
 *
 * @param ss_socket O socket do Servidor de Status.
 * @param client_id O ID deste cliente.
 * @return int Retorna 1 para continuar a execução, 0 em caso de erro fatal.
 */
int handle_check_failure(int ss_socket, int client_id)
{
    printf("Sending REQ_SENSSTATUS %d\n", client_id);
    Msg_t disc = {0};
    disc.type = REQ_SENSSTATUS;
    disc.payload = client_id;

    if (send_msg(ss_socket, &disc) == -1)
    {
        printf("Error sending check failure request\n");
        close(ss_socket);
        return 0;
    }

    Msg_t resp = {0};

    if (recv_msg(ss_socket, &resp) <= 0)
    {
        printf("Error receiving check failure response\n");
        return 0;
    }

    if (resp.type == ERROR_MSG)
    {
        printf("%s\n", resp.desc);
        return 1;
    }

    if (resp.type == RES_SENSSTATUS)
    {
        int location = resp.payload;
        char area[BUFSZ];

        switch (location)
        {
        case 1:
        case 2:
        case 3:
            strcpy(area, "1 (Norte)");
            break;
        case 4:
        case 5:
            strcpy(area, "2 (Sul)");
            break;
        case 6:
        case 7:
            strcpy(area, "3 (Leste)");
            break;
        case 8:
        case 9:
        case 10:
            strcpy(area, "4 (Oeste)");
            break;
        default:
            strcpy(area, "Unknown");
            break;
        }

        printf("Alert received from area: %s\n", area);
    }

    if (resp.type == OK_MSG)
    {
        printf("%s\n", resp.desc);
    }

    return 1;
}

/**
 * @brief Processa o comando 'locate'.
 *
 * Envia uma requisição (`REQ_SENSLOC`) para o Servidor de Localização (SL)
 * para obter a localização do sensor com o ID especificado e imprime a resposta.
 *
 * @param sl_socket O socket do Servidor de Localização.
 * @param client_id O ID do sensor a ser localizado.
 * @return int Retorna 1 para continuar, -1 em caso de erro.
 */
int handle_locate_sensor(int sl_socket, int client_id)
{
    printf("Sending REQ_SENSLOC %d\n", client_id);
    Msg_t disc = {0};
    disc.type = REQ_SENSLOC;
    disc.payload = client_id;

    if (send_msg(sl_socket, &disc) == -1)
    {
        printf("Error sending locate sensor request\n");
        close(sl_socket);
        return -1;
    }

    Msg_t resp = {0};
    int recv = recv_msg(sl_socket, &resp);

    if (recv <= 0)
    {
        printf("Error receiving locate sensor response\n");
        return -1;
    }

    if (resp.type == ERROR_MSG)
    {
        printf("%s\n", resp.desc);
        return 1;
    }

    if (resp.type == RES_SENSLOC)
    {
        printf("Current sensor location: %d\n", resp.payload);
    }

    return 1;
}

/**
 * @brief Processa o comando 'diagnose'.
 *
 * Envia uma requisição (`REQ_LOCLIST`) para o Servidor de Localização (SL) para
 * obter uma lista de todos os sensores em uma determinada localização e imprime o resultado.
 *
 * @param sl_socket O socket do Servidor de Localização.
 * @param loc_id O ID da localização a ser diagnosticada.
 * @return int Retorna 1 para continuar, -1 em caso de erro.
 */
int handle_diagnose_loc(int sl_socket, int loc_id)
{
    printf("Sending REQ_LOCLIST %d\n", loc_id);
    Msg_t disc = {0};
    disc.type = REQ_LOCLIST;
    disc.payload = loc_id;

    if (send_msg(sl_socket, &disc) == -1)
    {
        printf("Error sending diagnose location request\n");
        close(sl_socket);
        return -1;
    }

    Msg_t resp = {0};
    int recv = recv_msg(sl_socket, &resp);

    if (recv == 0)
    {
        printf("Error receiving diagnose location response\n");
        return -1;
    }

    if (resp.type == ERROR_MSG)
    {
        printf("%s\n", resp.desc);
        return 1;
    }

    if (resp.type == RES_LOCLIST)
    {
        printf("Sensors at location %d: %s\n", loc_id, resp.desc);
    }

    return 1;
}

/**
 * @brief Aguarda por atividade nos sockets ou na entrada padrão.
 *
 * Utiliza `select()` para monitorar a entrada do usuário (stdin) e os sockets
 * dos dois servidores (SS e SL). Processa os comandos do usuário ('kill',
 * 'check failure', 'locate', 'diagnose') e direciona para a função de
 * tratamento correspondente.
 *
 * @param read_fds Conjunto de file descriptors a serem monitorados.
 * @param ss_socket O socket do Servidor de Status.
 * @param sl_socket O socket do Servidor de Localização.
 * @param client_id O ID deste cliente.
 * @return int Retorna 1 para continuar, 0 para encerrar, -1 em caso de erro.
 */
int wait_for_activity(fd_set *read_fds, int ss_socket, int sl_socket, int client_id)
{
    FD_ZERO(read_fds);
    FD_SET(STDIN_FILENO, read_fds);
    FD_SET(ss_socket, read_fds);
    FD_SET(sl_socket, read_fds);

    int max_fd = ss_socket > sl_socket ? ss_socket : sl_socket;
    if (max_fd < STDIN_FILENO)
    {
        max_fd = STDIN_FILENO;
    }

    if (select(max_fd + 1, read_fds, NULL, NULL, NULL) < 0)
    {
        logexit("select");
    }

    if (FD_ISSET(STDIN_FILENO, read_fds))
    {
        char buf[BUFSZ];
        memset(buf, 0, BUFSZ);
        if (fgets(buf, sizeof(buf), stdin) != NULL)
        {
            toLowerString(buf);
            if (strncmp(buf, "kill", 4) == 0)
            {
                return handle_kill(ss_socket, sl_socket, client_id);
            }

            if (strncmp(buf, "check failure", 13) == 0)
            {
                return handle_check_failure(ss_socket, client_id);
            }

            if (strncmp(buf, "locate", 6) == 0)
            {
                int id;
                if (sscanf(buf + 6, "%d", &id) == 1)
                {
                    return handle_locate_sensor(sl_socket, id);
                }
            }

            if (strncmp(buf, "diagnose", 8) == 0)
            {
                int loc_id;
                if (sscanf(buf + 8, "%d", &loc_id) == 1)
                {
                    return handle_diagnose_loc(sl_socket, loc_id);
                }
            }
        }
    }

    if (FD_ISSET(ss_socket, read_fds))
    {
        return handle_server_activity(ss_socket);
    }

    if (FD_ISSET(sl_socket, read_fds))
    {
        return handle_server_activity(sl_socket);
    }

    return 1;
}

/**
 * @brief Função principal do cliente.
 *
 * O cliente se conecta a duas portas de servidor. Ele não sabe qual é o Servidor
 * de Status (SS) e qual é o de Localização (SL). Após a conexão, ele envia uma
 * mensagem de identificação para ambos e, com base na resposta, determina qual
 * socket corresponde a qual servidor. Em seguida, entra em um loop para
 * processar comandos do usuário e atividade dos servidores.
 */
int main(int argc, char **argv)
{
    // Verifica se os argumentos da linha de comando estão corretos
    if (argc < 3)
    {
        usage(argc, argv);
    }

    int s, s_2;
    int ss_socket, sl_socket; // Sockets para Servidor de Status e Servidor de Localização
    struct sockaddr_storage storage;
    struct sockaddr_storage storage_2;
    char buf[BUFSZ];

    fd_set read_fds;
    // Inicializa as estruturas de endereço para os dois servidores
    if (0 != client_sockaddr_init(argv[1], argv[2], argv[3], &storage, &storage_2))
    {
        usage(argc, argv);
    }

    // Cria um socket para cada servidor
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    s_2 = socket(storage_2.ss_family, SOCK_STREAM, 0);

    if (s == -1 || s_2 == -1)
    {
        logexit("socket");
    }

    // Conecta-se aos dois servidores
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

    memset(buf, 0, BUFSZ);

    // Prepara a mensagem de conexão
    Msg_t msg1 = {0}, msg2 = {0};
    int client_id = get_client_id(); // Obtém um ID único para o cliente

    msg1.type = msg2.type = REQ_CONNSEN;
    msg1.payload = msg2.payload = client_id;

    // Envia a requisição de conexão para ambos os servidores
    send_msg(s, &msg1);
    send_msg(s_2, &msg2);

    memset(&msg1, 0, sizeof(msg1));

    // Recebe a resposta de ambos os servidores para identificá-los
    recv_msg(s, &msg1);
    recv_msg(s_2, &msg2);

    // Trata possíveis erros na conexão
    if (msg1.type == ERROR_MSG)
    {
        close(s);
        logexit(msg1.desc);
    }

    if (msg2.type == ERROR_MSG)
    {
        close(s_2);
        logexit(msg2.desc);
    }

    if (msg1.type != RES_CONNSEN || msg2.type != RES_CONNSEN)
    {
        close(s);
        close(s_2);
        logexit("Unexpected message type");
    }

    // Identifica qual servidor é o de Status (SS) e qual é o de Localização (SL)
    // com base na descrição enviada na resposta.
    if (strncmp(msg1.desc, "SS", 2) == 0)
    {
        ss_socket = s;
        sl_socket = s_2;
    }
    else
    {
        ss_socket = s_2;
        sl_socket = s;
    }

    printf("%s New ID: %d\n", msg1.desc, msg1.payload);
    printf("%s New ID: %d\n", msg2.desc, msg2.payload);
    printf("Ok(02)\n");

    // Loop principal: continua executando enquanto 'keep' for verdadeiro
    int keep = 1;
    while (keep)
    {
        keep = wait_for_activity(&read_fds, ss_socket, sl_socket, client_id);

        if (keep == 0) // Comando 'kill' foi bem-sucedido
        {
            break;
        }

        if (keep == -1) // Erro irrecuperável
        {
            return EXIT_FAILURE;
        }
    }

    // Fecha os sockets antes de sair
    close(s);
    close(s_2);

    exit(EXIT_SUCCESS);
}