/**
 * ============================================================================
 * Mini-Projeto Integrador (Parte 2: Cliente da Bancada) — aluno_cliente.c
 * Bibliotecas utilizadas: Clic.h + Cnet.h
 * ============================================================================
 *
 * COMO FUNCIONA:
 *   1. Usa Network_discoverServers() da biblioteca Cnet para encontrar todos
 *      os servidores ativos no laboratorio e guardar em um vetor ServerInfo[].
 *   2. Usa Table_create() e Table_select() da biblioteca Clic para mostrar
 *      os servidores encontrados em uma tabela interativa onde o aluno escolhe
 *      a sala com as setas do teclado!
 *   3. Envia a bancada, nome e duvida do aluno para a fila do professor e
 *      aguarda na tela a chamada de atendimento.
 *
 * COMO COMPILAR:
 *   No Linux:   gcc aluno_cliente.c Clic.c Cnet.c -o aluno_cliente
 *   No Windows: gcc aluno_cliente.c Clic.c Cnet.c -lws2_32 -o aluno_cliente.exe
 */

#include <stdio.h>
#include <string.h>
#include "Clic.h"
#include "Cnet.h"

#define MAX_SERVIDORES 10

int main(void) {
    Clic_printScreenBox();
    Clic_move(2, 1);
    Clic_setFontColor(Color_YELLOW);
    Clic_printCenter("BANCADA DO ALUNO - PROCURANDO SERVIDOR DO LABORATORIO NA REDE...");
    Clic_resetColor();
    fflush(stdout);

    /* 1. Cnet descobre os servidores na rede local e preenche o vetor 'lista' */
    ServerInfo lista[MAX_SERVIDORES];
    int encontrados = Network_discoverServers(lista, MAX_SERVIDORES);

    if (encontrados == 0) {
        Clic_move(5, 4);
        Clic_setFontColor(Color_RED);
        printf("Nenhum servidor encontrado na rede local!");
        Clic_move(7, 4);
        Clic_setFontColor(Color_WHITE);
        printf("Verifique se o professor ja iniciou o programa 'professor_servidor'.");
        Clic_resetColor();
        Clic_fixDraw();
        return 1;
    }

    /* 2. Clic monta uma tabela interativa com os servidores encontrados pela Cnet */
    Table *tabelaServidores = Table_create("%-28s %-18s %-8d");
    Table_setWidth(tabelaServidores, 62);
    Table_setHeader(tabelaServidores, "Sala / Professor", "Endereco IP", "Porta");

    for (int i = 0; i < encontrados; i++) {
        Table_addRow(tabelaServidores, lista[i].name, lista[i].ip, lista[i].port);
    }

    Clic_move(4, 1);
    Clic_printCenter("Escolha o servidor da sua turma com as setas e tecle [ENTER]:");
    Clic_move(6, 1);
    int linhaEscolhida = Table_select(tabelaServidores);
    Table_free(tabelaServidores);

    if (linhaEscolhida < 2) {
        Clic_fixDraw();
        return 0;
    }

    /* Conecta ao ServerInfo correspondente (linha 2 da tabela = indice 0 do vetor) */
    int indiceVetor = linhaEscolhida - 2;
    Client *cli = Client_connectInfo(lista[indiceVetor]);
    if (cli == NULL) {
        Clic_move(16, 4);
        Clic_setFontColor(Color_RED);
        printf("Erro ao conectar ao servidor selecionado.");
        Clic_resetColor();
        Clic_fixDraw();
        return 1;
    }

    /* 3. Formulario visual para o aluno preencher sua duvida */
    Clic_clearScreen();
    Clic_printScreenBox();

    Clic_move(2, 1);
    Clic_setFontColor(Color_YELLOW);
    Clic_printCenter("REGISTRAR PEDIDO DE AJUDA NA FILA DO PROFESSOR");
    Clic_resetColor();

    char bancada[32];
    char nome[40];
    char duvida[100];

    Clic_move(5, 5);
    printf("Identificacao da Bancada (ex: PC-04): ");
    if (!fgets(bancada, sizeof(bancada), stdin)) bancada[0] = '\0';
    bancada[strcspn(bancada, "\r\n")] = '\0';

    Clic_move(7, 5);
    printf("Seu Nome: ");
    if (!fgets(nome, sizeof(nome), stdin)) nome[0] = '\0';
    nome[strcspn(nome, "\r\n")] = '\0';

    Clic_move(9, 5);
    printf("Qual sua duvida? (ex: Laco while na Questao 2): ");
    if (!fgets(duvida, sizeof(duvida), stdin)) duvida[0] = '\0';
    duvida[strcspn(duvida, "\r\n")] = '\0';

    /* 4. Envia os dados separados por '|' em uma unica linha TCP */
    char pacote[Client_MESSAGE_SIZE];
    snprintf(pacote, sizeof(pacote), "%s|%s|%s",
             bancada[0] ? bancada : "PC-01",
             nome[0]    ? nome    : "Aluno",
             duvida[0]  ? duvida  : "Duvida geral");
    Client_sendLine(cli, pacote);

    /* 5. Exibe a confirmacao e acompanha os avisos de atendimento em tempo real */
    char resposta[Client_MESSAGE_SIZE];
    if (Client_receiveLine(cli, resposta) >= 0) {
        Clic_move(12, 5);
        Clic_setFontColor(Color_GREEN);
        printf("%s %s", Symbol_SMILE, resposta);
        Clic_resetColor();
    }

    Clic_move(14, 5);
    Clic_setFontColor(Color_CYAN);
    printf("Aguarde na sua bancada. Avisos do professor aparecerao abaixo ([ENTER] para sair):");
    Clic_resetColor();
    fflush(stdout);

    int linhaAviso = 16;
    while (Client_isConnected(cli)) {
        if (Client_receiveLineNonBlocking(cli, resposta) > 0) {
            if (linhaAviso < Clic_getScreenHeight() - 2) {
                Clic_move(linhaAviso++, 5);
            } else {
                Clic_move(16, 5);
                Clic_clearLine();
            }
            Clic_setFontColor(Color_YELLOW);
            printf("%s %s", Symbol_ARROW, resposta);
            Clic_resetColor();
            fflush(stdout);
        }

        int tecla = Clic_keyCaptureNonBlocking();
        if (tecla == Key_ENTER || tecla == Key_ESC) {
            break;
        }
        Clic_pause(0.08f);
    }

    Clic_fixDraw();
    Client_close(cli);
    return 0;
}