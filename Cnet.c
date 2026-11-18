/**
 * ============================================================================
 * Cnet.c — Implementacao Multiplataforma (Linux e Windows) da Biblioteca Cnet
 * Autor: Prof. Paulo Regis M. Sousa
 * ============================================================================
 *
 * COMO FUNCIONA POR DENTRO (NOTAS PARA OS ALUNOS):
 *   1. Protocolo TCP e Buffer de Linha: Na rede TCP, os dados chegam como um
 *      fluxo continuo de bytes (uma mensagem grande pode chegar em dois pedacos,
 *      ou duas mensagens curtas podem chegar juntas). Por isso, cada struct Client
 *      possui um 'lineBuffer' que guarda os bytes recebidos ate encontrar o
 *      caractere '\n' (fim de linha), entregando cada mensagem certinha!
 *   2. Descoberta na Rede Local (UDP Broadcast): Quando o aluno chama
 *      Network_discoverServers(), a biblioteca envia um pacote UDP com o texto
 *      "CLIC_DISCOVER" para a rede local (255.255.255.255) e para a propria
 *      maquina (127.0.0.1) na porta 39000. Todos os servidores com Discovery
 *      ativo respondem "CLIC_SERVER|Nome|Porta".
 *   3. Independencia Total: Este arquivo NAO inclui "Clic.h". Toda a lista de
 *      servidores descobertos e devolvida no vetor ServerInfo[].
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "Cnet.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * 1. Camada de Compatibilidade de Sockets: Windows (Winsock2) vs Linux (POSIX)
 * ------------------------------------------------------------------------- */

#ifdef _WIN32
    #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    typedef int ssize_t;
    typedef ULONG nfds_t;
    #define Cnet_CLOSE_SOCKET(s) closesocket((SOCKET)(s))
    #define Cnet_SHUT_RDWR       SD_BOTH
    #define Cnet_POLL(fds, n, t) WSAPoll((fds), (ULONG)(n), (t))
#else
    #include <fcntl.h>
    #include <poll.h>
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #define Cnet_CLOSE_SOCKET(s) close(s)
    #define Cnet_SHUT_RDWR       SHUT_RDWR
    #define Cnet_POLL(fds, n, t) poll((fds), (nfds_t)(n), (t))
#endif

#define Cnet_INVALID_SOCKET     (-1)
#define Cnet_DISCOVERY_QUERY    "CLIC_DISCOVER"
#define Cnet_DISCOVERY_RESPONSE "CLIC_SERVER"

/**
 * Inicializa a biblioteca de rede do sistema operacional (necessario no Windows).
 * Chamada automaticamente pelas funcoes publicas para que o aluno nao precise
 * configurar WSAStartup manualmente.
 */
static bool Network_init(void) {
    static bool initialized = false;
    if (initialized) {
        return true;
    }
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif
    initialized = true;
    return true;
}

static bool Network_isRetryableError(void) {
#ifdef _WIN32
    int err = WSAGetLastError();
    return err == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

static bool Network_isWouldBlockError(void) {
#ifdef _WIN32
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
#endif
}

static int Network_prepareSocket(int fd) {
#ifdef SO_NOSIGPIPE
    int enabled = 1;
    return setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&enabled, sizeof(enabled));
#else
    (void)fd;
    return 0;
#endif
}

static long long Network_milliseconds(void) {
#ifdef _WIN32
    return (long long)GetTickCount64();
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        return -1;
    }
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

static int Network_poll(struct pollfd *fds, nfds_t count, int timeout) {
    long long deadline = timeout >= 0 ? Network_milliseconds() + timeout : 0;
    for (;;) {
        int result = Cnet_POLL(fds, count, timeout);
        if (result >= 0 || !Network_isRetryableError()) {
            return result;
        }
        if (timeout >= 0) {
            long long remaining = deadline - Network_milliseconds();
            timeout = remaining > 0 ? (int)remaining : 0;
        }
    }
}

static void Network_copyString(char *destination, size_t destinationSize,
                               const char *source) {
    if (!destination || destinationSize == 0) {
        return;
    }
    if (!source) {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, destinationSize, "%s", source);
}

/**
 * Valida se uma string recebida pela rede contem apenas texto imprimivel,
 * sem depender de Clic_textWidth().
 */
static bool Network_isPrintableName(const char *text) {
    if (!text || *text == '\0') {
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if (*p < 32 || *p == 127) {
            return false;
        }
    }
    return true;
}

static int Network_setNonBlocking(int socketFD) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket((SOCKET)socketFD, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(socketFD, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(socketFD, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int Network_setBlocking(int socketFD) {
#ifdef _WIN32
    u_long mode = 0;
    return ioctlsocket((SOCKET)socketFD, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(socketFD, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(socketFD, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

static void Network_closeSocket(int *socketFD) {
    if (socketFD && *socketFD != Cnet_INVALID_SOCKET) {
        int savedError = errno;
        shutdown(*socketFD, Cnet_SHUT_RDWR);
        Cnet_CLOSE_SOCKET(*socketFD);
        *socketFD = Cnet_INVALID_SOCKET;
        errno = savedError;
    }
}

static bool Network_isValidPort(int port) {
    return port > 0 && port <= 65535;
}

/* -------------------------------------------------------------------------
 * 2. Funcoes Internas do Cliente e Gerenciamento do Buffer de Linha
 * ------------------------------------------------------------------------- */

static Client *Client_createFromSocket(int socketFD,
                                       const struct sockaddr_in *address) {
    Client *client = calloc(1, sizeof(Client));
    if (!client) {
        Cnet_CLOSE_SOCKET(socketFD);
        return NULL;
    }

    if (Network_prepareSocket(socketFD) == -1) {
        int savedError = errno;
        Cnet_CLOSE_SOCKET(socketFD);
        free(client);
        errno = savedError;
        return NULL;
    }

    client->socket = socketFD;
    client->connected = true;
    client->lineBufferLength = 0;
    client->name[0] = '\0';

    if (address) {
        if (!inet_ntop(AF_INET, (void *)&address->sin_addr, client->ip,
                       sizeof(client->ip))) {
            Network_copyString(client->ip, sizeof(client->ip), "0.0.0.0");
        }
        client->port = ntohs(address->sin_port);
    } else {
        struct sockaddr_in peer;
        socklen_t peerLength = sizeof(peer);
        memset(&peer, 0, sizeof(peer));

        if (getpeername(socketFD, (struct sockaddr *)&peer, &peerLength) == 0) {
            if (!inet_ntop(AF_INET, (void *)&peer.sin_addr, client->ip,
                           sizeof(client->ip))) {
                Network_copyString(client->ip, sizeof(client->ip), "0.0.0.0");
            }
            client->port = ntohs(peer.sin_port);
        } else {
            Network_copyString(client->ip, sizeof(client->ip), "0.0.0.0");
            client->port = 0;
        }
    }

    return client;
}

static int Network_connectSocket(const char *host, int port,
                                 int timeoutMilliseconds) {
    if (!Network_init() || !host || !Network_isValidPort(port)) {
        errno = EINVAL;
        return Cnet_INVALID_SOCKET;
    }

    char portText[6];
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *current;
    int socketFD = Cnet_INVALID_SOCKET;

    snprintf(portText, sizeof(portText), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, portText, &hints, &result) != 0) {
        errno = EHOSTUNREACH;
        return Cnet_INVALID_SOCKET;
    }

    for (current = result; current; current = current->ai_next) {
        socketFD = (int)socket(current->ai_family, current->ai_socktype,
                               current->ai_protocol);
        if (socketFD < 0) {
            socketFD = Cnet_INVALID_SOCKET;
            continue;
        }

        if (timeoutMilliseconds < 0) {
            if (connect(socketFD, current->ai_addr, (socklen_t)current->ai_addrlen) == 0) {
                break;
            }
            Cnet_CLOSE_SOCKET(socketFD);
            socketFD = Cnet_INVALID_SOCKET;
            continue;
        }

        if (Network_setNonBlocking(socketFD) == -1) {
            Cnet_CLOSE_SOCKET(socketFD);
            socketFD = Cnet_INVALID_SOCKET;
            continue;
        }

        if (connect(socketFD, current->ai_addr, (socklen_t)current->ai_addrlen) == 0) {
            Network_setBlocking(socketFD);
            break;
        }

        if (Network_isWouldBlockError()) {
            struct pollfd fd = { .fd = socketFD, .events = POLLOUT };
            int ready = Network_poll(&fd, 1, timeoutMilliseconds);
            if (ready > 0) {
                int connectError = 0;
                socklen_t errorLength = sizeof(connectError);
                if (getsockopt(socketFD, SOL_SOCKET, SO_ERROR,
                               (char *)&connectError, &errorLength) == 0 &&
                    connectError == 0) {
                    Network_setBlocking(socketFD);
                    break;
                }
                if (connectError) {
                    errno = connectError;
                }
            } else if (ready == 0) {
                errno = ETIMEDOUT;
            }
        }

        Cnet_CLOSE_SOCKET(socketFD);
        socketFD = Cnet_INVALID_SOCKET;
    }

    freeaddrinfo(result);
    return socketFD;
}

static int Client_sendBytes(Client *client, const char *data, size_t length) {
    size_t totalSent = 0;

    if (!Client_isConnected(client) || (!data && length > 0)) {
        errno = ENOTCONN;
        return -1;
    }

    while (totalSent < length) {
        ssize_t sent = send(client->socket, data + totalSent,
                            (int)(length - totalSent),
#ifdef MSG_NOSIGNAL
                            MSG_NOSIGNAL
#else
                            0
#endif
        );

        if (sent > 0) {
            totalSent += (size_t)sent;
            continue;
        }

        if (sent == -1 && Network_isRetryableError()) {
            continue;
        }

        Client_disconnect(client);
        return -1;
    }

    return (int)totalSent;
}

static int Client_copyBufferedData(Client *client, char *buffer) {
    if (!client || !buffer || client->lineBufferLength <= 0) {
        return 0;
    }

    int amount = client->lineBufferLength;
    if (amount > Client_MESSAGE_SIZE - 1) {
        amount = Client_MESSAGE_SIZE - 1;
    }

    memcpy(buffer, client->lineBuffer, (size_t)amount);
    buffer[amount] = '\0';

    memmove(client->lineBuffer, client->lineBuffer + amount,
            (size_t)(client->lineBufferLength - amount));
    client->lineBufferLength -= amount;

    return amount;
}

static void Client_appendBufferedData(Client *client, const char *data,
                                      int dataLength) {
    if (!client || !data || dataLength <= 0) {
        return;
    }
    assert(dataLength <= Client_LINE_BUFFER_SIZE - 1 - client->lineBufferLength);
    memcpy(client->lineBuffer + client->lineBufferLength, data, (size_t)dataLength);
    client->lineBufferLength += dataLength;
    client->lineBuffer[client->lineBufferLength] = '\0';
}

/**
 * Procura um '\n' dentro do lineBuffer do cliente.
 * Se encontrar, copia a linha (sem '\r' e sem '\n') para 'buffer' e desloca
 * o restante dos bytes para o inicio do lineBuffer.
 */
static int Client_extractLine(Client *client, char *buffer, bool *found) {
    int lineEnd = -1;

    if (found) {
        *found = false;
    }
    if (!client || !buffer) {
        return -1;
    }

    for (int index = 0; index < client->lineBufferLength; index++) {
        if (client->lineBuffer[index] == '\n') {
            lineEnd = index;
            break;
        }
    }

    if (lineEnd < 0) {
        return 0;
    }

    if (found) {
        *found = true;
    }

    int amount = lineEnd;
    if (amount > Client_MESSAGE_SIZE - 1) {
        amount = Client_MESSAGE_SIZE - 1;
    }

    memcpy(buffer, client->lineBuffer, (size_t)amount);
    buffer[amount] = '\0';

    /* Remove '\r' caso a mensagem tenha vindo com quebra de linha Windows (\r\n) */
    if (amount > 0 && buffer[amount - 1] == '\r') {
        buffer[amount - 1] = '\0';
        amount--;
    }

    memmove(client->lineBuffer, client->lineBuffer + lineEnd + 1,
            (size_t)(client->lineBufferLength - lineEnd - 1));
    client->lineBufferLength -= lineEnd + 1;
    client->lineBuffer[client->lineBufferLength] = '\0';

    return amount;
}

static int Client_waitReadable(Client *client, bool nonBlocking) {
    if (!Client_isConnected(client)) {
        errno = ENOTCONN;
        return -1;
    }
    struct pollfd fd = { .fd = client->socket, .events = POLLIN };
    return Network_poll(&fd, 1, nonBlocking ? 0 : -1);
}

static int Client_receiveRaw(Client *client, char *buffer, bool nonBlocking) {
    if (buffer) {
        buffer[0] = '\0';
    }
    if (!buffer || !Client_isConnected(client)) {
        errno = ENOTCONN;
        return -1;
    }

    int buffered = Client_copyBufferedData(client, buffer);
    if (buffered > 0) {
        return buffered;
    }

    int ready = Client_waitReadable(client, nonBlocking);
    if (ready == 0) {
        return Client_NO_DATA;
    }
    if (ready < 0) {
        Client_disconnect(client);
        return -1;
    }

    while (true) {
        ssize_t received = recv(client->socket, buffer,
                                Client_MESSAGE_SIZE - 1, 0);

        if (received > 0) {
            buffer[received] = '\0';
            return (int)received;
        }
        if (received == 0) {
            Client_disconnect(client);
            return -1;
        }
        if (Network_isRetryableError()) {
            continue;
        }
        if (nonBlocking && Network_isWouldBlockError()) {
            return Client_NO_DATA;
        }

        Client_disconnect(client);
        return -1;
    }
}

static int Client_receiveLineInternal(Client *client, char *buffer,
                                      bool nonBlocking) {
    char temporary[1024];

    if (buffer) {
        buffer[0] = '\0';
    }
    if (!buffer || !Client_isConnected(client)) {
        errno = ENOTCONN;
        return -1;
    }

    while (true) {
        bool found = false;
        int lineLength = Client_extractLine(client, buffer, &found);
        if (found) {
            return lineLength;
        }

        int ready = Client_waitReadable(client, nonBlocking);
        if (ready == 0) {
            return Client_NO_DATA;
        }
        if (ready < 0) {
            Client_disconnect(client);
            return -1;
        }

        while (true) {
            int available = Client_LINE_BUFFER_SIZE - 1 - client->lineBufferLength;
            if (available == 0) {
                Client_disconnect(client);
                errno = EMSGSIZE;
                return -1;
            }

            size_t capacity = (size_t)available < sizeof(temporary)
                ? (size_t)available : sizeof(temporary);

            ssize_t received = recv(client->socket, temporary, (int)capacity, 0);

            if (received > 0) {
                Client_appendBufferedData(client, temporary, (int)received);
                break;
            }
            if (received == 0) {
                Client_disconnect(client);
                return -1;
            }
            if (Network_isRetryableError()) {
                continue;
            }
            if (nonBlocking && Network_isWouldBlockError()) {
                return Client_NO_DATA;
            }

            Client_disconnect(client);
            return -1;
        }
    }
}

/* -------------------------------------------------------------------------
 * 3. Implementacao da API Publica do Cliente
 * ------------------------------------------------------------------------- */

Client *Client_connect(const char *ip, int port) {
    int socketFD = Network_connectSocket(ip, port, -1);
    if (socketFD == Cnet_INVALID_SOCKET) {
        return NULL;
    }
    return Client_createFromSocket(socketFD, NULL);
}

Client *Client_connectInfo(ServerInfo info) {
    return Client_connect(info.ip, info.port);
}

void Client_disconnect(Client *client) {
    if (!client) {
        return;
    }
    Network_closeSocket(&client->socket);
    client->connected = false;
    client->lineBufferLength = 0;
}

void Client_free(Client *client) {
    if (!client) {
        return;
    }
    Client_disconnect(client);
    free(client);
}

void Client_close(Client *client) {
    Client_free(client);
}

bool Client_isConnected(Client *client) {
    return client && client->connected &&
           client->socket != Cnet_INVALID_SOCKET;
}

int Client_send(Client *client, const char *message) {
    if (!message) {
        errno = EINVAL;
        return -1;
    }
    return Client_sendBytes(client, message, strlen(message));
}

int Client_sendLine(Client *client, const char *message) {
    if (!message) {
        errno = EINVAL;
        return -1;
    }

    int messageLength = Client_send(client, message);
    if (messageLength < 0) {
        return -1;
    }

    if (messageLength > 0 && message[messageLength - 1] == '\n') {
        return messageLength;
    }

    int sent = Client_sendBytes(client, "\n", 1);
    if (sent < 0) {
        return -1;
    }

    return messageLength + sent;
}

int Client_receive(Client *client, char *buffer) {
    return Client_receiveRaw(client, buffer, false);
}

int Client_receiveLine(Client *client, char *buffer) {
    return Client_receiveLineInternal(client, buffer, false);
}

int Client_receiveNonBlocking(Client *client, char *buffer) {
    return Client_receiveRaw(client, buffer, true);
}

int Client_receiveLineNonBlocking(Client *client, char *buffer) {
    return Client_receiveLineInternal(client, buffer, true);
}

char *Client_getIP(Client *client) {
    static char empty[] = "";
    return client ? client->ip : empty;
}

int Client_getPort(Client *client) {
    return client ? client->port : 0;
}

void Client_setName(Client *client, const char *name) {
    if (!client) {
        return;
    }
    Network_copyString(client->name, sizeof(client->name), name);
}

char *Client_getName(Client *client) {
    static char empty[] = "";
    return client ? client->name : empty;
}

/* -------------------------------------------------------------------------
 * 4. Implementacao da API Publica do Servidor
 * ------------------------------------------------------------------------- */

static void Server_removeClientAt(Server *server, int index,
                                  bool notifyDisconnect) {
    if (!server || index < 0 || index >= server->nClients) {
        return;
    }

    Client *client = server->clients[index];

    for (int current = index; current < server->nClients - 1; current++) {
        server->clients[current] = server->clients[current + 1];
    }
    server->clients[server->nClients - 1] = NULL;
    server->nClients--;

    if (notifyDisconnect && server->disconnectCallback && client) {
        server->disconnectCallback(client);
    }
    Client_free(client);
}

static void Server_cleanupDisconnectedClients(Server *server) {
    if (!server) {
        return;
    }
    for (int index = 0; index < server->nClients; index++) {
        Client *client = server->clients[index];
        if (!Client_isConnected(client)) {
            Server_removeClientAt(server, index, true);
            index--;
        }
    }
}

static Client *Server_addClient(Server *server, int socketFD,
                                const struct sockaddr_in *address) {
    if (!server || server->nClients >= Server_MAX_CLIENTS) {
        Cnet_CLOSE_SOCKET(socketFD);
        errno = ENOSPC;
        return NULL;
    }

    Client *client = Client_createFromSocket(socketFD, address);
    if (!client) {
        return NULL;
    }

    server->clients[server->nClients++] = client;

    if (server->connectCallback) {
        server->connectCallback(client);
    }

    return client;
}

static Client *Server_acceptOne(Server *server) {
    struct sockaddr_in address;
    socklen_t addressLength = sizeof(address);
    int clientSocket;

    if (!server || server->socket == Cnet_INVALID_SOCKET) {
        errno = ENOTCONN;
        return NULL;
    }

    memset(&address, 0, sizeof(address));

    do {
        clientSocket = (int)accept(server->socket, (struct sockaddr *)&address,
                                   &addressLength);
    } while (clientSocket == -1 && Network_isRetryableError());

    if (clientSocket < 0) {
        return NULL;
    }

    return Server_addClient(server, clientSocket, &address);
}

static void Server_processDiscovery(Server *server) {
    char request[256];
    struct sockaddr_in sender;
    socklen_t senderLength = sizeof(sender);

    if (!server || !server->discoveryEnabled ||
        server->discoverySocket == Cnet_INVALID_SOCKET) {
        return;
    }

    while (true) {
        char response[Client_NAME_SIZE + 32];
        memset(&sender, 0, sizeof(sender));
        senderLength = sizeof(sender);

        ssize_t received = recvfrom(server->discoverySocket, request,
                                    sizeof(request) - 1, 0,
                                    (struct sockaddr *)&sender, &senderLength);

        if (received < 0) {
            if (Network_isWouldBlockError()) {
                return;
            }
            if (Network_isRetryableError()) {
                continue;
            }
            return;
        }

        request[received] = '\0';
        if (strcmp(request, Cnet_DISCOVERY_QUERY) != 0) {
            continue;
        }

        snprintf(response, sizeof(response), "%s|%s|%d",
                 Cnet_DISCOVERY_RESPONSE, server->name, server->port);

        sendto(server->discoverySocket, response, (int)strlen(response), 0,
               (struct sockaddr *)&sender, senderLength);
    }
}

Server *Server_create(int port) {
    if (!Network_init() || port < 0 || port > 65535) {
        errno = EINVAL;
        return NULL;
    }

    Server *server = calloc(1, sizeof(Server));
    if (!server) {
        return NULL;
    }

    server->socket = (int)socket(AF_INET, SOCK_STREAM, 0);
    server->discoverySocket = Cnet_INVALID_SOCKET;

    if (server->socket < 0) {
        free(server);
        return NULL;
    }

    int reuseAddress = 1;
    setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuseAddress, sizeof(reuseAddress));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(server->socket, (struct sockaddr *)&address, sizeof(address)) == -1 ||
        listen(server->socket, Server_MAX_CLIENTS) == -1) {
        Cnet_CLOSE_SOCKET(server->socket);
        free(server);
        return NULL;
    }

    if (port == 0) {
        socklen_t addressLength = sizeof(address);
        if (getsockname(server->socket, (struct sockaddr *)&address,
                        &addressLength) == 0) {
            server->port = ntohs(address.sin_port);
        }
    } else {
        server->port = port;
    }

    char hostName[Client_NAME_SIZE] = {0};
    if (gethostname(hostName, sizeof(hostName)) == 0 && hostName[0] != '\0') {
        hostName[sizeof(hostName) - 1] = '\0';
        Server_setName(server, hostName);
    } else {
        Server_setName(server, "Cnet Server");
    }

    return server;
}

void Server_close(Server *server) {
    if (!server) {
        return;
    }

    Server_disableDiscovery(server);

    while (server->nClients > 0) {
        Server_removeClientAt(server, 0, false);
    }

    Network_closeSocket(&server->socket);
    free(server);
}

int Server_clientCount(Server *server) {
    if (!server) {
        return 0;
    }
    Server_cleanupDisconnectedClients(server);
    return server->nClients;
}

Client *Server_getClient(Server *server, int index) {
    if (!server) {
        return NULL;
    }
    Server_cleanupDisconnectedClients(server);
    if (index < 0 || index >= server->nClients) {
        errno = EINVAL;
        return NULL;
    }
    return server->clients[index];
}

Client *Server_accept(Server *server) {
    if (!server || server->socket == Cnet_INVALID_SOCKET) {
        errno = ENOTCONN;
        return NULL;
    }
    Server_cleanupDisconnectedClients(server);
    for (;;) {
        struct pollfd fds[2];
        fds[0].fd = server->socket;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        nfds_t nfds = 1;
        if (server->discoveryEnabled && server->discoverySocket != Cnet_INVALID_SOCKET) {
            fds[1].fd = server->discoverySocket;
            fds[1].events = POLLIN;
            fds[1].revents = 0;
            nfds = 2;
        }

        if (Network_poll(fds, nfds, -1) < 0) {
            return NULL;
        }
        if (nfds == 2 && (fds[1].revents & POLLIN)) {
            Server_processDiscovery(server);
        }
        if (fds[0].revents & POLLIN) {
            return Server_acceptOne(server);
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            errno = EIO;
            return NULL;
        }
    }
}

Client *Server_acceptNonBlocking(Server *server) {
    if (!server || server->socket == Cnet_INVALID_SOCKET) {
        errno = ENOTCONN;
        return NULL;
    }
    Server_update(server);
    struct pollfd fd = { .fd = server->socket, .events = POLLIN };
    if (Network_poll(&fd, 1, 0) > 0 && (fd.revents & POLLIN)) {
        return Server_acceptOne(server);
    }
    return NULL;
}

void Server_send(Server *server, Client *client, const char *message) {
    (void)server;
    if (!client || !message) {
        return;
    }
    Client_send(client, message);
}

void Server_sendAll(Server *server, const char *message) {
    if (!server || !message) {
        return;
    }
    for (int index = 0; index < server->nClients; index++) {
        Client_send(server->clients[index], message);
    }
    Server_cleanupDisconnectedClients(server);
}

void Server_sendOthers(Server *server, Client *origin, const char *message) {
    if (!server || !message) {
        return;
    }
    for (int index = 0; index < server->nClients; index++) {
        Client *client = server->clients[index];
        if (client != origin) {
            Client_send(client, message);
        }
    }
    Server_cleanupDisconnectedClients(server);
}

static Client *Server_deliver(Server *server, Client *client, char *buffer) {
    if (server->messageCallback) {
        server->messageCallback(client, buffer);
    }
    for (int i = 0; i < server->nClients; i++) {
        if (server->clients[i] == client) {
            return client;
        }
    }
    return NULL;
}

static Client *Server_receiveInternal(Server *server, char *buffer,
                                      bool nonBlocking, bool receiveLine) {
    if (buffer) {
        buffer[0] = '\0';
    }
    if (!server || !buffer || server->socket == Cnet_INVALID_SOCKET) {
        errno = EINVAL;
        return NULL;
    }
    Server_cleanupDisconnectedClients(server);

    for (;;) {
        for (int i = 0; i < server->nClients; i++) {
            Client *client = server->clients[i];
            bool found = false;
            int length = receiveLine ? Client_extractLine(client, buffer, &found)
                                     : Client_copyBufferedData(client, buffer);
            if (found || (!receiveLine && length > 0)) {
                return Server_deliver(server, client, buffer);
            }
        }

        struct pollfd fds[Server_MAX_CLIENTS + 2];
        memset(fds, 0, sizeof(fds));

        fds[0].fd = server->socket;
        fds[0].events = POLLIN;

        int offset = 1;
        int discoverySlot = -1;
        if (server->discoveryEnabled && server->discoverySocket != Cnet_INVALID_SOCKET) {
            discoverySlot = offset++;
            fds[discoverySlot].fd = server->discoverySocket;
            fds[discoverySlot].events = POLLIN;
        }

        int clientCount = server->nClients;
        for (int i = 0; i < clientCount; i++) {
            fds[offset + i].fd = server->clients[i]->socket;
            fds[offset + i].events = POLLIN;
        }

        if (Network_poll(fds, (nfds_t)(offset + clientCount), nonBlocking ? 0 : -1) <= 0) {
            return NULL;
        }

        if (discoverySlot >= 0 && (fds[discoverySlot].revents & POLLIN)) {
            Server_processDiscovery(server);
        }

        for (int i = 0; i < clientCount; i++) {
            if (!fds[offset + i].revents) {
                continue;
            }
            Client *client = server->clients[i];
            int received = receiveLine ? Client_receiveLineInternal(client, buffer, true)
                                       : Client_receiveRaw(client, buffer, true);
            if ((receiveLine && received >= 0) || (!receiveLine && received > 0)) {
                return Server_deliver(server, client, buffer);
            }
        }

        Server_cleanupDisconnectedClients(server);

        if (fds[0].revents & POLLIN) {
            Server_acceptOne(server);
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            errno = EIO;
            return NULL;
        }
        if (nonBlocking) {
            return NULL;
        }
    }
}

Client *Server_receive(Server *server, char *buffer) {
    return Server_receiveInternal(server, buffer, false, false);
}

Client *Server_receiveLine(Server *server, char *buffer) {
    return Server_receiveInternal(server, buffer, false, true);
}

Client *Server_receiveNonBlocking(Server *server, char *buffer) {
    return Server_receiveInternal(server, buffer, true, false);
}

Client *Server_receiveLineNonBlocking(Server *server, char *buffer) {
    return Server_receiveInternal(server, buffer, true, true);
}

void Server_onConnect(Server *server, ServerConnectCallback callback) {
    if (server) server->connectCallback = callback;
}

void Server_onDisconnect(Server *server, ServerDisconnectCallback callback) {
    if (server) server->disconnectCallback = callback;
}

void Server_onMessage(Server *server, ServerMessageCallback callback) {
    if (server) server->messageCallback = callback;
}

/* -------------------------------------------------------------------------
 * 5. Descoberta Automatica de Servidores na Rede Local (100% Desacoplada)
 * ------------------------------------------------------------------------- */

bool Server_enableDiscovery(Server *server) {
    if (!Network_init() || !server) {
        errno = EINVAL;
        return false;
    }
    if (server->discoveryEnabled) {
        return true;
    }

    int discoverySocket = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (discoverySocket < 0) {
        return false;
    }

    int reuseAddress = 1;
    setsockopt(discoverySocket, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuseAddress, sizeof(reuseAddress));
#ifdef SO_REUSEPORT
    setsockopt(discoverySocket, SOL_SOCKET, SO_REUSEPORT,
               (const char *)&reuseAddress, sizeof(reuseAddress));
#endif

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(Network_DISCOVERY_PORT);

    if (bind(discoverySocket, (struct sockaddr *)&address, sizeof(address)) == -1 ||
        Network_setNonBlocking(discoverySocket) == -1) {
        Cnet_CLOSE_SOCKET(discoverySocket);
        return false;
    }

    server->discoverySocket = discoverySocket;
    server->discoveryEnabled = true;
    return true;
}

void Server_disableDiscovery(Server *server) {
    if (!server) {
        return;
    }
    Network_closeSocket(&server->discoverySocket);
    server->discoveryEnabled = false;
}

void Server_setName(Server *server, const char *name) {
    if (!server) {
        return;
    }
    Network_copyString(server->name, sizeof(server->name), name);
    for (char *p = server->name; *p; p++) {
        if (*p == '|' || (unsigned char)*p < 32 || *p == 127) {
            *p = ' ';
        }
    }
}

char *Server_getName(Server *server) {
    static char empty[] = "";
    return server ? server->name : empty;
}

void Server_update(Server *server) {
    if (!server) {
        return;
    }
    Server_processDiscovery(server);
    Server_cleanupDisconnectedClients(server);
}

static void Network_addDiscoveredServer(ServerInfo list[], int *count,
                                        int maxServers, const char *name,
                                        const char *ip, int port) {
    if (!list || !count || *count >= maxServers || !Network_isPrintableName(name) ||
        strlen(name) >= Client_NAME_SIZE || !ip || !Network_isValidPort(port)) {
        return;
    }

    /* Evita duplicar o mesmo servidor caso ele responda via broadcast e loopback */
    for (int i = 0; i < *count; i++) {
        if (list[i].port == port && strcmp(list[i].ip, ip) == 0) {
            return;
        }
    }

    Network_copyString(list[*count].name, sizeof(list[*count].name), name);
    Network_copyString(list[*count].ip,   sizeof(list[*count].ip),   ip);
    list[*count].port = port;
    (*count)++;
}

int Network_discoverServers(ServerInfo list[], int maxServers) {
    if (!Network_init() || !list || maxServers <= 0) {
        errno = EINVAL;
        return 0;
    }

    int discoverySocket = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (discoverySocket < 0) {
        return 0;
    }

    int broadcast = 1;
    setsockopt(discoverySocket, SOL_SOCKET, SO_BROADCAST,
               (const char *)&broadcast, sizeof(broadcast));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(Network_DISCOVERY_PORT);
    address.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    const char query[] = Cnet_DISCOVERY_QUERY;
    ssize_t broadcastSent = sendto(discoverySocket, query, (int)strlen(query), 0,
                                   (struct sockaddr *)&address, sizeof(address));

    /* Tambem consulta 127.0.0.1 para funcionar em testes feitos na mesma maquina */
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ssize_t localSent = sendto(discoverySocket, query, (int)strlen(query), 0,
                               (struct sockaddr *)&address, sizeof(address));

    if (broadcastSent < 0 && localSent < 0) {
        Cnet_CLOSE_SOCKET(discoverySocket);
        return 0;
    }

    int count = 0;
    long long start = Network_milliseconds();

    while (count < maxServers) {
        long long remaining = Network_DISCOVERY_TIME - (Network_milliseconds() - start);
        if (remaining <= 0) {
            break;
        }

        struct pollfd fd = { .fd = discoverySocket, .events = POLLIN };
        int result = Network_poll(&fd, 1, (int)remaining);
        if (result <= 0) {
            break;
        }

        if (fd.revents & POLLIN) {
            char response[Client_NAME_SIZE + 40];
            struct sockaddr_in sender;
            socklen_t senderLength = sizeof(sender);
            ssize_t received = recvfrom(discoverySocket, response,
                                        sizeof(response) - 1, 0,
                                        (struct sockaddr *)&sender,
                                        &senderLength);

            if (received <= 0) {
                continue;
            }
            response[received] = '\0';

            char *kind     = strtok(response, "|");
            char *name     = strtok(NULL, "|");
            char *portText = strtok(NULL, "|");
            char senderIP[40];

            if (!kind || !name || !portText ||
                strcmp(kind, Cnet_DISCOVERY_RESPONSE) != 0) {
                continue;
            }

            if (!inet_ntop(AF_INET, (void *)&sender.sin_addr, senderIP,
                           sizeof(senderIP))) {
                continue;
            }

            char *end = NULL;
            errno = 0;
            long port = strtol(portText, &end, 10);
            if (errno != 0 || !end || *end != '\0' || port <= 0 || port > 65535) {
                continue;
            }

            Network_addDiscoveredServer(list, &count, maxServers,
                                        name, senderIP, (int)port);
        }
    }

    Cnet_CLOSE_SOCKET(discoverySocket);
    return count;
}