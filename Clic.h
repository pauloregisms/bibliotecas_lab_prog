/**
 * ============================================================================
 * Clic.h — Biblioteca Didatica de Interface de Terminal e Tabelas em C
 * Autor: Prof. Paulo Regis M. Sousa
 * ============================================================================
 *
 * O QUE ESTA BIBLIOTECA FAZ:
 *   Permite criar programas visuais no terminal usando cores, caixas (molduras),
 *   posicionamento de cursor, captura instantanea de teclado (setas, ENTER, DEL)
 *   e tabelas interativas selecionaveis.
 *
 * COMPATIBILIDADE:
 *   Funciona nativamente no Linux, macOS e Windows (Windows 10/11 via MinGW/GCC).
 *   Totalmente independente da biblioteca de rede (Cnet.h).
 *
 * COMO COMPILAR:
 *   gcc meu_programa.c Clic.c -o meu_programa
 */

#ifndef CLIC_H
#define CLIC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * 1. Simbolos Unicode para Desenho no Terminal (Codificacao UTF-8)
 * ------------------------------------------------------------------------- */

#define Symbol_HLINE        "\u2501"     /* Linha horizontal grossa: ━ */
#define Symbol_VLINE        "\u2503"     /* Linha vertical grossa:   ┃ */
#define Symbol_CORNER_TL    "\u250f"     /* Canto superior esquerdo: ┏ */
#define Symbol_CORNER_TR    "\u2513"     /* Canto superior direito:  ┓ */
#define Symbol_CORNER_BL    "\u2517"     /* Canto inferior esquerdo: ┗ */
#define Symbol_CORNER_BR    "\u251b"     /* Canto inferior direito:  ┛ */
#define Symbol_T_SIDE_L     "\u2523"     /* Conector T esquerdo:     ┣ */
#define Symbol_T_SIDE_R     "\u252b"     /* Conector T direito:      ┫ */
#define Symbol_T_UP         "\u2533"     /* Conector T superior:     ┳ */
#define Symbol_T_DOWN       "\u253b"     /* Conector T inferior:     ┻ */
#define Symbol_CROSS        "\u254b"     /* Cruzamento central:      ╋ */
#define Symbol_ARROW        "\u2192"     /* Seta para a direita:     → */
#define Symbol_ARROW_RETURN "\u21b3"     /* Seta de retorno:         ↳ */
#define Symbol_THREE_DOTS   "\u2026"     /* Reticencias:             … */
#define Symbol_TREE_POINTS  "\u2026"     /* Alias mantido por compatibilidade */
#define Symbol_DIAG         "\u2572"     /* Diagonal principal:      ╲ */
#define Symbol_CDIAG        "\u2571"     /* Diagonal secundaria:     ╱ */
#define Symbol_BLOCK        "\u2588"     /* Bloco solido cheio:      █ */
#define Symbol_SAD          "\U0001f641" /* Rostinho triste:        🙁 */
#define Symbol_SMILE        "\U0001f642" /* Rostinho feliz:         🙂 */

/** Quantidade maxima de colunas suportadas em uma Tabela. */
#define Row_MAX_CELLS 60

/* -------------------------------------------------------------------------
 * 2. Paleta de Cores (Padrao ANSI 256 Cores) e Codigos de Teclas
 * ------------------------------------------------------------------------- */

/**
 * Cores disponiveis para usar em Clic_setFontColor() e Clic_setBackgroundColor().
 */
typedef enum Color {
    Color_BLACK     = 0,
    Color_GRAY      = 8,
    Color_RED       = 9,
    Color_GREEN     = 10,
    Color_YELLOW    = 11,
    Color_MAGENTA   = 13,
    Color_CYAN      = 14,
    Color_CIAN      = 14, /* Alias mantido para nao quebrar codigos antigos */
    Color_WHITE     = 15,
    Color_BLUE      = 39,
    Color_ORANGE    = 208,
    Color_LIGHTGRAY = 248
} Color;

/**
 * Codigos retornados pelas funcoes Clic_keyCapture() e Clic_keyCaptureNonBlocking().
 */
typedef enum Key {
    Key_TAB    = 9,
    Key_ENTER  = 10,
    Key_ESC    = 27,
    Key_SPACE  = 32,
    Key_UP     = 65,
    Key_DOWN   = 66,
    Key_RIGHT  = 67,
    Key_LEFT   = 68,
    Key_DELETE = 127,
    Key_0 = 48, Key_1 = 49, Key_2 = 50, Key_3 = 51, Key_4 = 52,
    Key_5 = 53, Key_6 = 54, Key_7 = 55, Key_8 = 56, Key_9 = 57,
    Key_A = 97,  Key_B = 98,  Key_C = 99,  Key_D = 100, Key_E = 101,
    Key_F = 102, Key_G = 103, Key_H = 104, Key_I = 105, Key_J = 106,
    Key_K = 107, Key_L = 108, Key_M = 109, Key_N = 110, Key_O = 111,
    Key_P = 112, Key_Q = 113, Key_R = 114, Key_S = 115, Key_T = 116,
    Key_U = 117, Key_V = 118, Key_W = 119, Key_X = 120, Key_Y = 121,
    Key_Z = 122
} Key;

/* -------------------------------------------------------------------------
 * 3. Estruturas da Tabela Interativa
 * ------------------------------------------------------------------------- */

typedef struct Format Format;
typedef struct Cell   Cell;
typedef struct Row    Row;
typedef struct Table  Table;

struct Format {
    char formatString[30];
    bool isLeftAligned;
    bool isCustomWidth;
    int  width;
};

struct Cell {
    char  *content;
    Format format;
};

struct Row {
    int  index;
    int  length;
    Cell cell[Row_MAX_CELLS];
    Row *next;
    Row *prev;
};

struct Table {
    int     nRows;
    int     nCols;
    int     width;
    int     defaultCellWidth;
    int     highlightedIndex;
    int     firstSelectableIndex;
    Color   highlightColor;
    Format *formats;
    Row    *firstRow;
    Row    *lastRow;
};

/* -------------------------------------------------------------------------
 * 4. API de Terminal — Cores
 * ------------------------------------------------------------------------- */

/**
 * Volta a cor do texto e do fundo para o padrao normal do terminal.
 * Dica: Chame sempre apos imprimir um texto colorido!
 *
 * Exemplo:
 *   Clic_setFontColor(Color_GREEN);
 *   printf("Aprovado!");
 *   Clic_resetColor();
 */
void Clic_resetColor(void);

/**
 * Muda a cor das proximas letras impressas com printf().
 * @param color Uma das cores do enum Color (ex: Color_YELLOW, Color_CYAN).
 */
void Clic_setFontColor(Color color);

/**
 * Muda a cor de fundo das proximas letras impressas com printf().
 * @param color Uma das cores do enum Color (ex: Color_BLUE, Color_RED).
 */
void Clic_setBackgroundColor(Color color);

/* -------------------------------------------------------------------------
 * 5. API de Terminal — Tela e Dimensoes
 * ------------------------------------------------------------------------- */

/**
 * Apaga todo o conteudo visivel da tela e leva o cursor para o topo (linha 1, coluna 1).
 */
void Clic_clearScreen(void);

/**
 * Apaga todo o texto da linha onde o cursor esta, sem mudar de linha.
 */
void Clic_clearLine(void);

/**
 * Imprime uma quebra de linha ('\n').
 */
void Clic_breakLine(void);

/**
 * Descobre quantas colunas de largura cabem na janela atual do terminal.
 * @return Largura em colunas (geralmente 80 ou mais).
 */
int Clic_getScreenWidth(void);

/**
 * Descobre quantas linhas de altura cabem na janela atual do terminal.
 * @return Altura em linhas (geralmente 24 ou mais).
 */
int Clic_getScreenHeight(void);

/* -------------------------------------------------------------------------
 * 6. API de Terminal — Posicionamento e Visibilidade do Cursor
 * ------------------------------------------------------------------------- */

/**
 * Move o cursor diretamente para a coordenada (line, column).
 * O canto superior esquerdo da tela e a linha 1, coluna 1.
 *
 * @param line   Numero da linha (1 ate Clic_getScreenHeight()).
 * @param column Numero da coluna (1 ate Clic_getScreenWidth()).
 *
 * Exemplo:
 *   Clic_move(5, 10);
 *   printf("Texto na linha 5, coluna 10");
 */
void Clic_move(int line, int column);

/** Move o cursor N linhas para cima a partir da posicao atual. */
void Clic_moveUp(int lines);

/** Move o cursor N linhas para baixo a partir da posicao atual. */
void Clic_moveDown(int lines);

/** Move o cursor N colunas para a direita a partir da posicao atual. */
void Clic_moveRight(int columns);

/** Move o cursor N colunas para a esquerda a partir da posicao atual. */
void Clic_moveLeft(int columns);

/** Move o cursor N linhas para baixo e posiciona na coluna 1. */
void Clic_moveDownBegin(int lines);

/** Move o cursor N linhas para cima e posiciona na coluna 1. */
void Clic_moveUpBegin(int lines);

/** Move o cursor para a coluna indicada mantendo a mesma linha. */
void Clic_moveToColumn(int column);

/** Move o cursor para o inicio (coluna 1) da linha atual. */
void Clic_moveToBegin(void);

/** Salva a posicao atual do cursor na memoria do terminal. */
void Clic_saveCursorPosition(void);

/** Restaura o cursor para a ultima posicao salva com Clic_saveCursorPosition(). */
void Clic_restoreCursorPosition(void);

/** Esconde o cursor piscante (muito util em jogos e paineis). */
void Clic_hideCursor(void);

/** Mostra novamente o cursor piscante no terminal. */
void Clic_showCursor(void);

/**
 * Posiciona o cursor no final da tela e pula uma linha.
 * Chame antes do 'return 0;' do main() para que o prompt do sistema nao
 * fique por cima das caixas que voce desenhou.
 */
void Clic_fixDraw(void);

/* -------------------------------------------------------------------------
 * 7. API de Terminal — Desenho de Caixas, Linhas e Texto Centralizado
 * ------------------------------------------------------------------------- */

/**
 * Desenha uma caixa (moldura ┏━━┓) a partir da posicao atual do cursor.
 *
 * @param width  Largura total da caixa em colunas (minimo 3).
 * @param height Altura total da caixa em linhas (minimo 3).
 *
 * Exemplo:
 *   Clic_move(3, 5);
 *   Clic_printBox(40, 8);
 */
void Clic_printBox(int width, int height);

/**
 * Limpa a tela e desenha uma moldura do tamanho exato da janela do terminal.
 */
void Clic_printScreenBox(void);

/** Desenha uma linha horizontal continua (━━━) com a largura informada. */
void Clic_printHLine(int width);

/** Desenha uma linha vertical continua (┃) com a altura informada. */
void Clic_printVLine(int height);

/** Desenha uma faixa horizontal preenchida com a cor de fundo atual. */
void Clic_printHBlockLine(int width);

/** Desenha uma faixa vertical preenchida com a cor de fundo atual. */
void Clic_printVBlockLine(int height);

/** Imprime um simbolo Unicode (ex: Symbol_ARROW, Symbol_SMILE). */
void Clic_printSymbol(const char *symbol);

/**
 * Calcula quantas colunas visuais um texto UTF-8 ocupa na tela,
 * tratando corretamente palavras acentuadas em portugues (ex: "Ação" = 4).
 *
 * @param text String UTF-8 terminada em '\0'.
 * @return     Numero de colunas visuais ou -1 se o texto for invalido.
 */
int Clic_textWidth(const char *text);

/**
 * Imprime um texto centralizado horizontalmente na linha atual do cursor.
 *
 * @param text Texto a ser centralizado.
 * @return     Coluna onde o texto comecou a ser impresso (ou 0 em caso de erro).
 *
 * Exemplo:
 *   Clic_move(2, 1);
 *   Clic_printCenter("BEM-VINDO AO JOGO");
 */
int Clic_printCenter(const char *text);

/* -------------------------------------------------------------------------
 * 8. API de Terminal — Captura de Teclado e Pausa
 * ------------------------------------------------------------------------- */

/**
 * Pausa o programa e espera o usuario apertar uma tecla (sem precisar dar ENTER).
 * Reconhece automaticamente as setas do teclado (Key_UP, Key_DOWN, Key_LEFT, Key_RIGHT),
 * além de Key_ENTER, Key_DELETE e letras normais.
 *
 * @return Codigo da tecla pressionada.
 */
int Clic_keyCapture(void);

/**
 * Verifica se o usuario apertou alguma tecla SEM travar o programa.
 * Ideal para lacos (while) de jogos ou paineis que atualizam a rede ao mesmo tempo.
 *
 * @return Codigo da tecla pressionada, ou EOF (-1) se nenhuma tecla foi apertada.
 */
int Clic_keyCaptureNonBlocking(void);

/**
 * Pausa a execucao do programa pelo tempo indicado em segundos.
 * Aceita valores decimais (ex: 0.5f pausa por meio segundo; 0.05f pausa por 50 ms).
 *
 * @param seconds Tempo de espera em segundos.
 */
void Clic_pause(float seconds);

/* -------------------------------------------------------------------------
 * 9. API de Tabelas Formatadas e Interativas
 * ------------------------------------------------------------------------- */

/**
 * Cria uma nova tabela definindo o formato de cada coluna no estilo printf().
 * Use "%-20s" para alinhar texto a esquerda com largura 20, "%d" para inteiros,
 * "%8.2f" para numeros reais com duas casas decimais.
 *
 * IMPORTANTE: Ao terminar de usar a tabela, libere a memoria com Table_free().
 *
 * @param format String de formatos separados por espaco (ex: "%-20s %-12s %6d").
 * @return       Ponteiro para a nova Table ou NULL em caso de erro.
 */
Table *Table_create(const char *format);

/**
 * Define a largura total da tabela em colunas (por padrao usa a largura da tela).
 */
void Table_setWidth(Table *table, int width);

/**
 * Adiciona uma linha de cabecalho fixa no topo da tabela e configura a selecao
 * interativa (Table_select) para pular o cabecalho automaticamente!
 *
 * Exemplo:
 *   Table *t = Table_create("%-20s %-15s %-6s");
 *   Table_setHeader(t, "Nome do Servidor", "Endereco IP", "Porta");
 */
void Table_setHeader(Table *table, ...);

/**
 * Adiciona uma linha de dados na tabela, passando um valor para cada coluna
 * conforme o formato escolhido em Table_create().
 *
 * Exemplo:
 *   Table_addRow(t, "Servidor Lab 01", "192.168.1.10", "5000");
 */
void Table_addRow(Table *table, ...);

/**
 * Desenha a tabela completa na tela a partir da linha atual do cursor.
 */
void Table_print(Table *table);

/**
 * Exibe a tabela e permite que o usuario escolha uma linha usando as setas
 * para CIMA e para BAIXO, confirmando com ENTER ou excluindo com DELETE.
 *
 * @param table Tabela criada com Table_create().
 * @return      Indice da linha escolhida (comecando em 1, ou em 2 se tiver cabecalho).
 *              Se o usuario apertar DELETE, retorna o indice NEGATIVO (ex: -2).
 *              Se o usuario apertar ESC ou a tabela estiver vazia, retorna -1.
 */
int Table_select(Table *table);

/**
 * Libera toda a memoria alocada para a tabela, suas linhas e textos.
 */
void Table_free(Table *table);

#endif /* CLIC_H */