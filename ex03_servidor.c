/**
 * Exemplo 03 — Servidor TCP com Descoberta na Rede Local (Usa apenas Cnet.h)
 *
 * Como compilar:
 *   No Linux:   gcc ex03_servidor.c Cnet.c -o ex03_servidor
 *   No Windows: gcc ex03_servidor.c Cnet.c -lws2_32 -o ex03_servidor.exe
 */
#include <stdio.h>
#include "Cnet.h"

int main(void) {
    /* 1. Abre o servidor TCP na porta 5000 */
    Server *srv = Server_create(5000);
    if (srv == NULL) {
        printf("Erro: Nao foi possivel abrir a porta 5000.\n");
        return 1;
    }

    /* 2. Define o nome do servidor e ativa o anuncio automatico na rede local */
    Server_setName(srv, "Servidor Eco - Lab C");
    Server_enableDiscovery(srv);

    printf("Servidor '%s' ativo na porta %d!\n", Server_getName(srv), srv->port);
    printf("Aguardando mensagens dos alunos (Pressione Ctrl+C para parar)...\n\n");

    char mensagem[Client_MESSAGE_SIZE];

    while (1) {
        /* 3. Aguarda uma linha enviada por qualquer cliente conectado */
        Client *autor = Server_receiveLine(srv, mensagem);

        if (autor != NULL) {
            printf("[Cliente %s:%d] enviou: %s\n",
                   Client_getIP(autor), Client_getPort(autor), mensagem);

            /* 4. Responde para todos os clientes conectados */
            char resposta[Client_MESSAGE_SIZE];
            snprintf(resposta, sizeof(resposta),
                     "Servidor recebeu de %.40s: %.180s\n",
                     Client_getIP(autor), mensagem);
            Server_sendAll(srv, resposta);
        }
    }

    Server_close(srv);
    return 0;
}