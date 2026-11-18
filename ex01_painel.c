/**
 * Exemplo 01 — Painel Visual, Cores e Molduras (Usa apenas Clic.h)
 *
 * Como compilar:
 *   gcc ex01_painel.c Clic.c -o ex01_painel
 */
#include "Clic.h"

int main(void) {
    /* 1. Limpa a tela e desenha uma moldura ocupando toda a janela */
    Clic_printScreenBox();

    /* 2. Escreve um titulo centralizado em amarelo na linha 3 */
    Clic_move(3, 1);
    Clic_setFontColor(Color_YELLOW);
    Clic_printCenter("PAINEL DE MONITORAMENTO DO LABORATORIO DE PROGRAMACAO");
    Clic_resetColor();

    /* 3. Desenha uma caixa central menor (largura 54, altura 9) */
    Clic_move(6, 14);
    Clic_setFontColor(Color_CYAN);
    Clic_printBox(54, 9);
    Clic_resetColor();

    /* 4. Preenche o interior da caixa com informacoes coloridas */
    Clic_move(8, 17);
    Clic_setFontColor(Color_GREEN);
    printf("%s Status dos Terminais: Operacional", Symbol_SMILE);

    Clic_move(10, 17);
    Clic_setFontColor(Color_ORANGE);
    printf("%s Dica: Use Clic_move(linha, coluna) para posicionar!", Symbol_ARROW);

    Clic_move(12, 17);
    Clic_setFontColor(Color_WHITE);
    printf("Pressione qualquer tecla para encerrar...");
    Clic_resetColor();

    /* 5. Aguarda o aluno apertar uma tecla e posiciona o cursor no rodape */
    Clic_keyCapture();
    Clic_fixDraw();
    return 0;
}