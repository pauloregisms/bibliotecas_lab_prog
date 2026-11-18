/**
 * ============================================================================
 * Mini-Projeto Integrador (Parte 1: Painel do Professor) — professor_servidor.c
 * Bibliotecas utilizadas: Clic.h + Cnet.h
 * ============================================================================
 *
 * DESCRICAO DO MINI-PROJETO:
 *   Sistema de "Fila de Duvidas do Laboratorio em Rede".
 *   - Os alunos abrem o programa 'aluno_cliente' em suas bancadas, encontram
 *     o servidor do professor automaticamente na rede e enviam sua duvida.
 *   - O computador do professor exibe uma Tabela em tempo real (Clic) com os
 *     chamados recebidos via TCP (Cnet).
 *   - Quando o professor aperta [ENTER], o primeiro aluno da fila e chamado
 *     e todos os computadores do laboratorio recebem o aviso de atendimento!
 *
 * COMO COMPILAR:
 *   No Linux:   gcc professor_servidor.c Clic.c Cnet.c -o professor_servidor
 *   No Windows: gcc professor_servidor.c Clic.c Cnet.c -lws2_32 -o professor_servidor.exe
 */

#include <stdio.h>
#include <string.h>
#include "Clic.h"
#include "Cnet.h"

#define MAX_CHAMADOS 30
#define PORTA_LAB    5000

typedef struct {
    int  ordem;
    char bancada[32];
    char aluno[40];
    char duvida[100];
    char ip[40];
    bool pendente;
} Chamado;

static void desenharTelaProfessor(Chamado fila[], int totalRegistros,
                                  int clientesOnline, const char *ultimoAviso) {
    Clic_clearScreen();

    /* Cabecalho do Painel */
    Clic_move(1, 1);
    Clic_setFontColor(Color_YELLOW);
    Clic_printCenter("FILA DE ATENDIMENTO DO LABORATORIO DE PROGRAMACAO EM C");
    Clic_resetColor();

    Clic_move(2, 1);
    Clic_setFontColor(Color_CYAN);
    Clic_printCenter("Comandos: [ENTER] Chamar proximo aluno da fila | [Q] Encerrar servidor");
    Clic_resetColor();

    /* Constroi a tabela com os chamados pendentes */
    Table *tabela = Table_create("%-6d %-10s %-20s %-32s %-15s");
    Table_setHeader(tabela, "Senha", "Bancada", "Estudante", "Topico / Duvida", "IP Bancada");

    int pendentes = 0;
    for (int i = 0; i < totalRegistros; i++) {
        if (fila[i].pendente) {
            Table_addRow(tabela,
                         fila[i].ordem,
                         fila[i].bancada,
                         fila[i].aluno,
                         fila[i].duvida,
                         fila[i].ip);
            pendentes++;
        }
    }

    Clic_move(4, 1);
    if (pendentes == 0) {
        Clic_setFontColor(Color_GREEN);
        Clic_printCenter("Nenhum chamado pendente no momento! Aguardando alunos na rede...");
        Clic_resetColor();
    } else {
        Table_print(tabela);
    }

    Table_free(tabela);

    /* Rodape com status da rede */
    int linhaRodape = Clic_getScreenHeight() - 2;
    Clic_move(linhaRodape, 2);
    Clic_setFontColor(Color_WHITE);
    printf("Bancadas conectadas: %d | Chamados na fila: %d | Status: %s",
           clientesOnline, pendentes, ultimoAviso);
    Clic_resetColor();
    fflush(stdout);
}

int main(void) {
    Server *srv = Server_create(PORTA_LAB);
    if (srv == NULL) {
        printf("Erro ao abrir a porta %d.\n", PORTA_LAB);
        return 1;
    }

    Server_setName(srv, "Lab Prog C - Prof. Paulo");
    Server_enableDiscovery(srv);

    Chamado fila[MAX_CHAMADOS];
    memset(fila, 0, sizeof(fila));
    int totalRegistros = 0;
    int ultimoTotalClientes = -1;
    char ultimoAviso[128] = "Servidor pronto e anunciando na rede local (UDP 39000).";
    bool precisaRedesenhar = true;

    Clic_hideCursor();

    while (1) {
        /* 1. Verifica sem bloquear se algum aluno enviou um chamado: BANCADA|ALUNO|DUVIDA */
        char msg[Client_MESSAGE_SIZE];
        Client *alunoCli = Server_receiveLineNonBlocking(srv, msg);

        if (alunoCli != NULL && totalRegistros < MAX_CHAMADOS) {
            char *bancada = strtok(msg, "|");
            char *nome    = strtok(NULL, "|");
            char *duvida  = strtok(NULL, "|");

            if (bancada && nome && duvida) {
                Chamado *novo = &fila[totalRegistros];
                novo->ordem    = totalRegistros + 1;
                novo->pendente = true;
                snprintf(novo->bancada, sizeof(novo->bancada), "%s", bancada);
                snprintf(novo->aluno,   sizeof(novo->aluno),   "%s", nome);
                snprintf(novo->duvida,  sizeof(novo->duvida),  "%s", duvida);
                snprintf(novo->ip,      sizeof(novo->ip),      "%s", Client_getIP(alunoCli));
                totalRegistros++;

                char confirmacao[Client_MESSAGE_SIZE];
                snprintf(confirmacao, sizeof(confirmacao),
                         "Confirmado! Sua senha na fila e #%d (%s).",
                         novo->ordem, novo->bancada);
                Client_sendLine(alunoCli, confirmacao);

                snprintf(ultimoAviso, sizeof(ultimoAviso),
                         "Novo chamado recebido de %s (%s)", novo->aluno, novo->bancada);
                precisaRedesenhar = true;
            }
        }

        /* 2. Verifica se o numero de clientes conectados mudou */
        int clientesAgora = Server_clientCount(srv);
        if (clientesAgora != ultimoTotalClientes) {
            ultimoTotalClientes = clientesAgora;
            precisaRedesenhar = true;
        }

        /* 3. Redesenha o painel se houve mudanca na fila ou na rede */
        if (precisaRedesenhar) {
            desenharTelaProfessor(fila, totalRegistros, clientesAgora, ultimoAviso);
            precisaRedesenhar = false;
        }

        /* 4. Verifica sem bloquear se o professor apertou ENTER ou Q */
        int tecla = Clic_keyCaptureNonBlocking();
        if (tecla == Key_ENTER) {
            for (int i = 0; i < totalRegistros; i++) {
                if (fila[i].pendente) {
                    fila[i].pendente = false;

                    char avisoRede[Client_MESSAGE_SIZE];
                    snprintf(avisoRede, sizeof(avisoRede),
                             "ATENDIMENTO: Senha #%d - Professor indo ate %s (%s)!\n",
                             fila[i].ordem, fila[i].bancada, fila[i].aluno);
                    Server_sendAll(srv, avisoRede);

                    snprintf(ultimoAviso, sizeof(ultimoAviso),
                             "Chamando senha #%d: %s (%s)",
                             fila[i].ordem, fila[i].aluno, fila[i].bancada);
                    precisaRedesenhar = true;
                    break;
                }
            }
        } else if (tecla == 'q' || tecla == 'Q' || tecla == Key_ESC) {
            break;
        }

        Clic_pause(0.05f);
    }

    Clic_showCursor();
    Clic_fixDraw();
    Server_close(srv);
    return 0;
}