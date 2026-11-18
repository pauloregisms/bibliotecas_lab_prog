/**
 * ============================================================================
 * Cnet.h — Biblioteca Didatica de Comunicacao em Rede TCP/IPv4 e Descoberta UDP
 * Autor: Prof. Paulo Regis M. Sousa
 * ============================================================================
 *
 * O QUE ESTA BIBLIOTECA FAZ:
 *   Facilita a criacao de programas Cliente/Servidor em C (chats, jogos em rede,
 *   sistemas de fila e paineis distribuidos) sem precisar configurar sockets
 *   complexos manualmente. Tambem permite encontrar servidores na rede local
 *   automaticamente via UDP!
 *
 * COMPATIBILIDADE E INDEPENDENCIA:
 *   - 100% independente da Clic.h (nao precisa de Clic.c para compilar).
 *   - Funciona nativamente no Linux, macOS e Windows (Winsock2 inicializado
 *     automaticamente nos bastidores).
 *
 * COMO COMPILAR:
 *   No Linux:   gcc meu_programa.c Cnet.c -o meu_programa
 *   No Windows: gcc meu_programa.c Cnet.c -lws2_32 -o meu_programa.exe
 */

#ifndef CNET_H
#define CNET_H

#include <stdbool.h>

/* -------------------------------------------------------------------------
 * 1. Constantes de Rede e Limites de Mensagens
 * ------------------------------------------------------------------------- */

/**
 * Retornado pelas funcoes *NonBlocking quando nenhuma mensagem chegou ainda.
 * Permite diferenciar "nenhuma mensagem nova" (-2) de "erro/desconexao" (-1).
 */
#define Client_NO_DATA          (-2)

/** Tamanho maximo para nomes de clientes e servidores (63 letras + '\0'). */
#define Client_NAME_SIZE        64

/** Tamanho recomendado para o vetor char buffer[Client_MESSAGE_SIZE]. */
#define Client_MESSAGE_SIZE     256

/** Tamanho do buffer interno de acumulacao de linha por cliente. */
#define Client_LINE_BUFFER_SIZE 4096

/** Numero maximo de clientes conectados simultaneamente no mesmo servidor. */
#define Server_MAX_CLIENTS      100

/** Porta UDP usada para localizar servidores automaticamente na rede local. */
#define Network_DISCOVERY_PORT  39000

/** Tempo de espera (em milissegundos) na busca por servidores na rede local. */
#define Network_DISCOVERY_TIME  1000

/* -------------------------------------------------------------------------
 * 2. Tipos e Estruturas de Conexao
 * ------------------------------------------------------------------------- */

typedef struct Client     Client;
typedef struct Server     Server;
typedef struct ServerInfo ServerInfo;

/**
 * Guarda os dados de um servidor encontrado na rede local por Network_discoverServers().
 * Voce pode acessar os campos .name, .ip e .port diretamente!
 */
struct ServerInfo {
    char name[Client_NAME_SIZE]; /**< Nome anunciado pelo servidor */
    char ip[40];                 /**< Endereco IPv4 (ex: "192.168.1.15") */
    int  port;                   /**< Porta TCP onde o servidor aceita conexoes */
};

typedef void (*ServerConnectCallback)(Client *client);
typedef void (*ServerDisconnectCallback)(Client *client);
typedef void (*ServerMessageCallback)(Client *client, char *message);

/**
 * Representa uma conexao TCP (usada tanto pelo cliente quanto pelo servidor).
 * Dica: Prefira usar as funcoes Client_getIP(), Client_getName() etc.
 */
struct Client {
    int  socket;
    char ip[40];
    int  port;
    bool connected;

    char name[Client_NAME_SIZE];
    char lineBuffer[Client_LINE_BUFFER_SIZE];
    int  lineBufferLength;
};

/**
 * Representa um servidor TCP capaz de atender ate Server_MAX_CLIENTS conexoes.
 */
struct Server {
    int     socket;
    int     port;
    int     nClients;
    Client *clients[Server_MAX_CLIENTS];

    ServerConnectCallback    connectCallback;
    ServerDisconnectCallback disconnectCallback;
    ServerMessageCallback    messageCallback;

    int  discoverySocket;
    bool discoveryEnabled;
    char name[Client_NAME_SIZE];
};

/* -------------------------------------------------------------------------
 * 3. API do Cliente — Conexao e Ciclo de Vida
 * ------------------------------------------------------------------------- */

/**
 * Conecta a um servidor TCP informando o endereco IP (ou "127.0.0.1") e a porta.
 *
 * @param ip   String com o IP ou nome da maquina (ex: "127.0.0.1" ou "192.168.0.10").
 * @param port Porta TCP do servidor (ex: 5000).
 * @return     Ponteiro para o Client conectado, ou NULL se nao conseguir conectar.
 *             IMPORTANTE: Feche com Client_free() (ou Client_close()) no final do programa!
 *
 * Exemplo:
 *   Client *cli = Client_connect("127.0.0.1", 5000);
 */
Client *Client_connect(const char *ip, int port);

/**
 * Conecta diretamente a um servidor encontrado por Network_discoverServers().
 *
 * @param info Estrutura ServerInfo preenchida pela busca na rede.
 * @return     Ponteiro para o Client conectado, ou NULL em caso de erro.
 *
 * Exemplo:
 *   ServerInfo lista[10];
 *   if (Network_discoverServers(lista, 10) > 0) {
 *       Client *cli = Client_connectInfo(lista[0]);
 *   }
 */
Client *Client_connectInfo(ServerInfo info);

/**
 * Fecha a conexao e libera toda a memoria do cliente criado com Client_connect().
 * ATENCAO: Nunca chame esta funcao em um cliente retornado pelo Server (o servidor
 * gerencia e libera seus clientes conectados automaticamente).
 */
void Client_free(Client *client);

/**
 * Alias didatico para Client_free(): fecha o socket e libera o cliente.
 */
void Client_close(Client *client);

/**
 * Desconecta o socket do cliente sem liberar a estrutura da memoria.
 * Pode ser usada dentro do servidor para desconectar um cliente especifico.
 */
void Client_disconnect(Client *client);

/**
 * Verifica se o cliente continua conectado.
 * @return true se estiver conectado, false se a conexao ja foi encerrada.
 */
bool Client_isConnected(Client *client);

/* -------------------------------------------------------------------------
 * 4. API do Cliente — Envio e Recebimento de Mensagens
 * ------------------------------------------------------------------------- */

/**
 * Envia uma mensagem de texto garantindo uma quebra de linha ('\n') no final.
 * Esta e a forma RECOMENDADA de enviar mensagens para parear com Client_receiveLine()
 * ou Server_receiveLine().
 *
 * @param client  Cliente conectado.
 * @param message String de texto a ser enviada.
 * @return        Total de bytes enviados, ou -1 se houver erro/desconexao.
 */
int Client_sendLine(Client *client, const char *message);

/**
 * Envia os bytes exatos da string sem adicionar '\n' no final.
 */
int Client_send(Client *client, const char *message);

/**
 * Aguarda ate chegar uma linha completa (terminada em '\n') e copia para 'buffer'
 * ja sem o '\n' no final.
 *
 * @param client Cliente conectado.
 * @param buffer Vetor char com pelo menos Client_MESSAGE_SIZE (256) posicoes.
 * @return       Tamanho da mensagem recebida (>= 0), ou -1 se desconectou.
 *
 * Exemplo:
 *   char msg[Client_MESSAGE_SIZE];
 *   if (Client_receiveLine(cli, msg) >= 0) printf("Chegou: %s\n", msg);
 */
int Client_receiveLine(Client *client, char *buffer);

/**
 * Verifica se ja chegou uma linha completa SEM travar o programa.
 *
 * @param client Cliente conectado.
 * @param buffer Vetor char com pelo menos Client_MESSAGE_SIZE (256) posicoes.
 * @return       Tamanho da linha (>= 0), Client_NO_DATA (-2) se ainda nao chegou
 *               nada, ou -1 se a conexao caiu.
 */
int Client_receiveLineNonBlocking(Client *client, char *buffer);

/** Recebe ate 255 bytes brutos (bloqueante, sem exigir '\n'). */
int Client_receive(Client *client, char *buffer);

/** Recebe ate 255 bytes brutos sem esperar; retorna Client_NO_DATA se vazio. */
int Client_receiveNonBlocking(Client *client, char *buffer);

/* -------------------------------------------------------------------------
 * 5. API do Cliente — Identificacao (IP, Porta e Nome)
 * ------------------------------------------------------------------------- */

/**
 * Retorna o endereco IP do cliente como string (ex: "127.0.0.1").
 * IMPORTANTE: Nao use free() no ponteiro retornado!
 */
char *Client_getIP(Client *client);

/** Retorna o numero da porta remota do cliente (ou 0 se client for NULL). */
int Client_getPort(Client *client);

/** Define um apelido/nome local para identificar o cliente (ate 63 letras). */
void Client_setName(Client *client, const char *name);

/**
 * Retorna o nome associado ao cliente.
 * IMPORTANTE: Nao use free() no ponteiro retornado!
 */
char *Client_getName(Client *client);

/* -------------------------------------------------------------------------
 * 6. API do Servidor — Criacao, Envio e Recebimento
 * ------------------------------------------------------------------------- */

/**
 * Cria e inicia um servidor TCP escutando na porta informada.
 *
 * @param port Numero da porta (ex: 5000). Se passar 0, o sistema escolhe uma porta livre.
 * @return     Ponteiro para o Server criado, ou NULL se a porta ja estiver em uso.
 *             Lembre-se de fechar com Server_close() no final do programa!
 */
Server *Server_create(int port);

/**
 * Encerra o servidor, desconecta todos os clientes e libera toda a memoria.
 */
void Server_close(Server *server);

/**
 * Retorna quantos clientes continuam conectados ao servidor neste instante.
 */
int Server_clientCount(Server *server);

/**
 * Retorna o cliente na posicao 'index' (de 0 ate Server_clientCount(s) - 1).
 * IMPORTANTE: O cliente pertence ao servidor (nao chame Client_free nele).
 */
Client *Server_getClient(Server *server, int index);

/**
 * Aguarda ate que QUALQUER cliente conectado envie uma linha ('\n') e coloca
 * o texto em 'buffer'. Enquanto espera, o servidor aceita novos clientes e
 * responde buscas de rede (Discovery) automaticamente!
 *
 * @param server Servidor ativo.
 * @param buffer Vetor char com pelo menos Client_MESSAGE_SIZE (256) posicoes.
 * @return       Ponteiro para o Client que enviou a mensagem, ou NULL em erro.
 */
Client *Server_receiveLine(Server *server, char *buffer);

/**
 * Verifica se algum cliente enviou uma linha completa SEM travar o programa.
 * Ideal para usar junto com Clic_keyCaptureNonBlocking() no loop principal!
 *
 * @return Ponteiro para o Client que enviou a linha, ou NULL se ninguem enviou nada.
 */
Client *Server_receiveLineNonBlocking(Server *server, char *buffer);

/** Aguarda bytes brutos de qualquer cliente (bloqueante). */
Client *Server_receive(Server *server, char *buffer);

/** Verifica bytes brutos de qualquer cliente sem bloquear; retorna NULL se vazio. */
Client *Server_receiveNonBlocking(Server *server, char *buffer);

/** Aguarda a entrada de um novo cliente especificamente (bloqueante). */
Client *Server_accept(Server *server);

/** Aceita um novo cliente pendente sem esperar, ou retorna NULL. */
Client *Server_acceptNonBlocking(Server *server);

/** Envia uma string para um cliente especifico do servidor. */
void Server_send(Server *server, Client *client, const char *message);

/**
 * Envia uma mensagem para TODOS os clientes conectados ao servidor (Broadcast).
 * Dica: Inclua '\n' no final da mensagem se os clientes usarem Client_receiveLine()!
 */
void Server_sendAll(Server *server, const char *message);

/**
 * Envia uma mensagem para todos os clientes, EXCETO para o cliente 'origin'.
 * Muito util em salas de bate-papo para nao repetir a mensagem para quem a enviou.
 */
void Server_sendOthers(Server *server, Client *origin, const char *message);

/* -------------------------------------------------------------------------
 * 7. API de Descoberta na Rede Local (UDP Broadcast + Loopback)
 * ------------------------------------------------------------------------- */

/**
 * Define o nome que o servidor mostrara quando os alunos buscarem servidores na rede.
 * @param server Servidor ativo.
 * @param name   Nome amigavel (ex: "Sala 01 - Prof. Paulo").
 */
void Server_setName(Server *server, const char *name);

/** Retorna o nome atual do servidor. */
char *Server_getName(Server *server);

/**
 * Ativa o anuncio do servidor na rede local (porta UDP 39000) para que os
 * clientes possam encontra-lo usando Network_discoverServers().
 *
 * @return true se ativou com sucesso, false em caso de erro.
 */
bool Server_enableDiscovery(Server *server);

/** Desativa o anuncio do servidor na rede local. */
void Server_disableDiscovery(Server *server);

/**
 * Processa buscas de rede pendentes e remove clientes desconectados sem bloquear.
 * (Nota: Server_receiveLine e Server_receiveLineNonBlocking ja chamam isso sozinhos).
 */
void Server_update(Server *server);

/**
 * Procura servidores ativos na rede local (e na propria maquina) durante 1 segundo.
 * Nao depende da biblioteca Clic! Preenche o vetor 'list' com os servidores achados.
 *
 * @param list       Vetor de estruturas ServerInfo declarado pelo aluno.
 * @param maxServers Quantidade maxima de posicoes do vetor 'list'.
 * @return           Numero total de servidores encontrados (0 se nenhum respondeu).
 *
 * Exemplo:
 *   ServerInfo encontrados[10];
 *   int qtd = Network_discoverServers(encontrados, 10);
 *   for (int i = 0; i < qtd; i++) {
 *       printf("%d) %s (%s:%d)\n", i+1, encontrados[i].name, encontrados[i].ip, encontrados[i].port);
 *   }
 */
int Network_discoverServers(ServerInfo list[], int maxServers);

/* -------------------------------------------------------------------------
 * 8. Callbacks Opcionais de Eventos no Servidor
 * ------------------------------------------------------------------------- */

void Server_onConnect(Server *server, ServerConnectCallback callback);
void Server_onDisconnect(Server *server, ServerDisconnectCallback callback);
void Server_onMessage(Server *server, ServerMessageCallback callback);

#endif /* CNET_H */