/**
 * Exemplo 02 — Tabela Formatada e Menu Interativo (Usa apenas Clic.h)
 *
 * Como compilar:
 *   gcc ex02_tabela.c Clic.c -o ex02_tabela
 */
#include "Clic.h"

int main(void) {
    Clic_clearScreen();
    Clic_move(1, 1);
    Clic_setFontColor(Color_YELLOW);
    Clic_printCenter("PAUTA DA TURMA - USE AS SETAS [CIMA/BAIXO] E TECLE [ENTER] OU [DEL]");
    Clic_resetColor();

    /* 1. Cria tabela com 3 colunas: Nome (texto), Matricula (inteiro), Media (real) */
    Table *turma = Table_create("%-26s %-12d %8.2f");
    Table_setWidth(turma, 56);

    /* 2. Define o cabecalho fixo (Table_select pula o cabecalho automaticamente!) */
    Table_setHeader(turma, "Nome do Estudante", "Matricula", "Media");

    /* 3. Adiciona os dados respeitando os tipos (%s, %d, %f) */
    Table_addRow(turma, "Ana Clara Oliveira",  202601, 9.50);
    Table_addRow(turma, "Bruno Mendes Costa",  202602, 7.85);
    Table_addRow(turma, "Carlos Eduardo Lima", 202603, 8.40);
    Table_addRow(turma, "Daniela Souza Rocha", 202604, 9.10);

    /* 4. Abre a selecao interativa a partir da linha 3 da tela */
    Clic_move(3, 1);
    int escolha = Table_select(turma);

    Clic_move(16, 1);
    if (escolha > 0) {
        Clic_setFontColor(Color_GREEN);
        printf("Voce confirmou (ENTER) o estudante da linha %d!\n", escolha);
    } else if (escolha < -1) {
        Clic_setFontColor(Color_RED);
        printf("Voce solicitou remover (DELETE) o estudante da linha %d!\n", -escolha);
    } else {
        printf("Selecao cancelada.\n");
    }
    Clic_resetColor();

    /* 5. Libera a memoria da tabela */
    Table_free(turma);
    return 0;
}