/**
 * Exemplo 04 — Cliente TCP com Busca Automatica na Rede (Usa apenas Cnet.h)
 *
 * Como compilar:
 *   No Linux:   gcc ex04_cliente.c Cnet.c -o ex04_cliente
 *   No Windows: gcc ex04_cliente.c Cnet.c -lws2_32 -o ex04_cliente.exe
 */
#include <stdio.h>
#include <string.h>
#include "Cnet.h"

int main(void) {
    printf("Procurando servidores ativos na rede local...\n");

    /* 1. Busca ate 10 servidores na rede local sem precisar da biblioteca Clic */
    ServerInfo lista[10];
    int encontrados = Network_discoverServers(lista, 10);

    Client *cli = NULL;
    if (encontrados > 0) {
        printf("Encontrado: %s (%s:%d). Conectando...\n",
               lista[0].name, lista[0].ip, lista[0].port);
        cli = Client_connectInfo(lista[0]);
    } else {
        printf("Nenhum servidor anunciado via UDP. Tentando 127.0.0.1:5000...\n");
        cli = Client_connect("127.0.0.1", 5000);
    }

    if (cli == NULL) {
        printf("Erro: Nao foi possivel conectar ao servidor.\n");
        return 1;
    }

    printf("Conectado com sucesso! Digite mensagens (ou 'sair' para terminar):\n");

    char texto[Client_MESSAGE_SIZE];
    char resposta[Client_MESSAGE_SIZE];

    while (Client_isConnected(cli)) {
        printf("> ");
        if (fgets(texto, sizeof(texto), stdin) == NULL) {
            break;
        }
        /* Remove o ENTER (\r ou \n) que o fgets deixa no final da string */
        texto[strcspn(texto, "\r\n")] = '\0';

        if (strcmp(texto, "sair") == 0) {
            break;
        }

        /* 2. Envia a linha para o servidor e aguarda a resposta */
        Client_sendLine(cli, texto);

        if (Client_receiveLine(cli, resposta) >= 0) {
            printf("%s\n", resposta);
        }
    }

    /* 3. Fecha a conexao e libera a memoria */
    Client_close(cli);
    return 0;
}