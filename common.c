#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include "common.h"

/**
 * @brief Exibe uma mensagem de erro e encerra o programa.
 * * Esta função utilitária imprime a mensagem de erro fornecida, seguida
 * pela descrição do erro do sistema (usando perror), e então termina
 * a execução do programa com status de falha.
 * * @param msg A mensagem de erro a ser exibida.
 */
void logexit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/**
 * @brief Inicializa duas estruturas de endereço de socket para o cliente.
 * * Prepara as estruturas `sockaddr_storage` para o cliente se conectar a dois
 * servidores (ou duas portas no mesmo servidor). Converte as strings de
 * endereço e porta para o formato binário de rede.
 * * @param addrstr String contendo o endereço IP do servidor.
 * @param portstr String contendo a porta do primeiro servidor.
 * @param portstr_2 String contendo a porta do segundo servidor.
 * @param storage Ponteiro para a estrutura que armazenará o endereço do primeiro servidor.
 * @param storage_2 Ponteiro para a estrutura que armazenará o endereço do segundo servidor.
 * @return int Retorna 0 em caso de sucesso, -1 em caso de falha (ex: argumentos nulos ou portas inválidas).
 */
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

/**
 * @brief Inicializa duas estruturas de endereço de socket para o servidor.
 * * Prepara as estruturas `sockaddr_storage` para o servidor: uma para a
 * comunicação P2P (peer-to-peer) e outra para a comunicação com os clientes.
 * * @param addrstr String contendo o endereço IP no qual o servidor irá operar.
 * @param portp2pstr String contendo a porta para a comunicação P2P.
 * @param portstr String contendo a porta para a comunicação com clientes.
 * @param server_storage Ponteiro para a estrutura que armazenará o endereço P2P.
 * @param clients_storage Ponteiro para a estrutura que armazenará o endereço dos clientes.
 * @return int Retorna 0 em caso de sucesso, -1 em caso de falha.
 */
int server_sockaddr_init(const char *addrstr, const char *portp2pstr, const char *portstr,
                         struct sockaddr_storage *server_storage, struct sockaddr_storage *clients_storage)
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
        struct sockaddr_in *p2p_addr4 = (struct sockaddr_in *)server_storage;
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

/**
 * @brief Envia uma estrutura Msg_t completa através de um socket.
 * * Função wrapper para a chamada `send` que garante que todos os bytes
 * da mensagem foram enviados e trata erros caso contrário.
 * * @param sock O file descriptor do socket para envio.
 * @param msg Ponteiro para a mensagem a ser enviada.
 * @return int O número de bytes enviados.
 */
int send_msg(int sock, Msg_t *msg)
{
    size_t count = send(sock, msg, sizeof(Msg_t), 0);
    if (count != sizeof(Msg_t))
    {
        logexit("send_msg");
    }

    return count;
}

/**
 * @brief Recebe uma estrutura Msg_t a partir de um socket.
 * * Função wrapper para a chamada `recv` que preenche a estrutura `msg`
 * fornecida com os dados recebidos e trata erros de recepção.
 * * @param sock O file descriptor do socket para recebimento.
 * @param msg Ponteiro para a estrutura onde a mensagem será armazenada.
 * @return int O número de bytes recebidos.
 */
int recv_msg(int sock, Msg_t *msg)
{
    size_t count = recv(sock, msg, sizeof(Msg_t), 0);
    if (count < 0)
    {
        logexit("recv_msg");
    }

    return count;
}

/**
 * @brief Converte todos os caracteres de uma string para minúsculas.
 * * Altera a string original, convertendo cada caractere para sua
 * versão em minúsculo.
 * * @param str A string a ser convertida.
 */
void toLowerString(char *str)
{
    for (char *p = str; *p; p++)
    {
        *p = tolower(*p);
    }
}