/**
 * ============================================================================
 * Clic.c — Implementacao Multiplataforma (Linux e Windows) da Biblioteca Clic
 * Autor: Prof. Paulo Regis M. Sousa
 * ============================================================================
 *
 * COMO FUNCIONA POR DENTRO (NOTAS PARA OS ALUNOS):
 *   1. Codigos ANSI ("\033[..."): Os terminais modernos entendem comandos
 *      especiais que comecam com o caractere ESC ('\033') seguido de '['.
 *      Por exemplo, "\033[2J" limpa a tela e "\033[5;10H" move o cursor para
 *      a linha 5, coluna 10.
 *   2. Portabilidade Linux / Windows: No Linux usamos <termios.h> e <sys/ioctl.h>.
 *      No Windows usamos <windows.h> e <conio.h>. Tudo fica escondido dentro
 *      deste arquivo para que o seu programa compile nos dois sistemas sem mudar
 *      uma unica linha!
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "Clic.h"
#include <limits.h>
#include <locale.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <fcntl.h>
#endif

/* Sequencia padrao ISO C para o caractere ESC (27 em decimal / 0x1B em hex) */
#define CLIC_ESC "\033"

/* -------------------------------------------------------------------------
 * 1. Inicializacao Automatica da Plataforma (UTF-8 e Modo ANSI no Windows)
 * ------------------------------------------------------------------------- */

static void Clic_initPlatform(void) {
    static bool initialized = false;
    if (initialized) {
        return;
    }

    setlocale(LC_CTYPE, "");

#ifdef _WIN32
    /* Configura o terminal do Windows para exibir acentos e simbolos UTF-8 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* Ativa o suporte nativo a sequencias ANSI (\033[...) no Windows 10/11 */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif

    initialized = true;
}

/* -------------------------------------------------------------------------
 * 2. Calculo Portatil de Largura de Texto UTF-8 (Substitui wcwidth POSIX)
 * ------------------------------------------------------------------------- */

/**
 * Em UTF-8, uma letra acentuada como 'ã' ou um simbolo como '┏' ocupa 2 ou 3
 * bytes na memoria (strlen retorna 2 ou 3), mas ocupa apenas 1 coluna na tela!
 * Bytes de continuacao UTF-8 sempre comecam com os bits '10xxxxxx' (0x80..0xBF).
 * Contando apenas os bytes iniciais, obtemos o numero de caracteres visiveis
 * tanto no Linux quanto no Windows, sem depender de wcwidth().
 */
int Clic_textWidth(const char *text) {
    if (!text) {
        return -1;
    }

    int width = 0;
    const unsigned char *p = (const unsigned char *)text;

    while (*p != '\0') {
        /* Rejeita caracteres de controle invisiveis (ASCII < 32 ou DEL = 127) */
        if (*p < 0x20 || *p == 0x7F) {
            return -1;
        }

        /* Se NAO for um byte de continuacao UTF-8 (10xxxxxx), soma 1 coluna */
        if ((*p & 0xC0) != 0x80) {
            if (width == INT_MAX) {
                return -1;
            }
            width++;
        }
        p++;
    }

    return width;
}

/**
 * Descobre quantos bytes do inicio de 'text' cabem dentro de 'maxWidth' colunas,
 * garantindo que nenhum caractere UTF-8 multibyte seja cortado pela metade.
 */
static size_t Text_prefix(const char *text, int maxWidth, int *outWidth) {
    size_t used = 0;
    int width = 0;
    const unsigned char *p = (const unsigned char *)text;

    while (p[used] != '\0') {
        size_t charBytes = 1;
        while (p[used + charBytes] != '\0' &&
               (p[used + charBytes] & 0xC0) == 0x80) {
            charBytes++;
        }

        if (width + 1 > maxWidth) {
            break;
        }

        width += 1;
        used += charBytes;
    }

    if (outWidth) {
        *outWidth = width;
    }
    return used;
}

static void Text_printCell(const char *text, int width, bool leftAligned) {
    if (width <= 0) {
        return;
    }

    int textWidth = Clic_textWidth(text);
    if (textWidth < 0) {
        text = "?";
        textWidth = 1;
    }

    size_t bytes = strlen(text);
    const char *suffix = "";
    int suffixWidth = 0;

    if (textWidth > width) {
        suffix = Symbol_THREE_DOTS;
        suffixWidth = Clic_textWidth(suffix);
        if (suffixWidth <= 0 || suffixWidth > width) {
            suffix = ".";
            suffixWidth = 1;
        }
        bytes = Text_prefix(text, width - suffixWidth, &textWidth);
    }

    int padding = width - textWidth - suffixWidth;
    if (!leftAligned) {
        printf("%*s", padding, "");
    }
    fwrite(text, 1, bytes, stdout);
    fputs(suffix, stdout);
    if (leftAligned) {
        printf("%*s", padding, "");
    }
}

/* -------------------------------------------------------------------------
 * 3. Cores e Atributos do Terminal
 * ------------------------------------------------------------------------- */

void Clic_resetColor(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "[m");
}

void Clic_setFontColor(Color color) {
    Clic_initPlatform();
    /* \033[38;5;<cor>m seleciona a cor da fonte na paleta de 256 cores */
    printf(CLIC_ESC "[38;5;%dm", (int)color);
}

void Clic_setBackgroundColor(Color color) {
    Clic_initPlatform();
    /* \033[48;5;<cor>m seleciona a cor de fundo na paleta de 256 cores */
    printf(CLIC_ESC "[48;5;%dm", (int)color);
}

/* -------------------------------------------------------------------------
 * 4. Movimentacao do Cursor e Limpeza de Tela
 * ------------------------------------------------------------------------- */

void Clic_move(int line, int column) {
    Clic_initPlatform();
    if (line < 1) line = 1;
    if (column < 1) column = 1;
    printf(CLIC_ESC "[%d;%dH", line, column);
}

void Clic_moveUp(int lines) {
    Clic_initPlatform();
    if (lines > 0) printf(CLIC_ESC "[%dA", lines);
}

void Clic_moveDown(int lines) {
    Clic_initPlatform();
    if (lines > 0) printf(CLIC_ESC "[%dB", lines);
}

void Clic_moveRight(int columns) {
    Clic_initPlatform();
    if (columns > 0) printf(CLIC_ESC "[%dC", columns);
}

void Clic_moveLeft(int columns) {
    Clic_initPlatform();
    if (columns > 0) printf(CLIC_ESC "[%dD", columns);
}

void Clic_moveDownBegin(int lines) {
    Clic_initPlatform();
    if (lines > 0) printf(CLIC_ESC "[%dE", lines);
}

void Clic_moveUpBegin(int lines) {
    Clic_initPlatform();
    if (lines > 0) printf(CLIC_ESC "[%dF", lines);
}

void Clic_moveToColumn(int column) {
    Clic_initPlatform();
    if (column < 1) column = 1;
    printf(CLIC_ESC "[%dG", column);
}

void Clic_moveToBegin(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "[1G");
}

void Clic_clearLine(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "[2K");
}

void Clic_clearScreen(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "[2J" CLIC_ESC "[1;1H");
}

void Clic_breakLine(void) {
    printf("\n");
}

void Clic_saveCursorPosition(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "7");
}

void Clic_restoreCursorPosition(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "8");
}

void Clic_hideCursor(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "[?25l");
}

void Clic_showCursor(void) {
    Clic_initPlatform();
    printf(CLIC_ESC "[?25h");
}

/* -------------------------------------------------------------------------
 * 5. Dimensoes da Janela do Terminal (Linux ioctl vs Windows Console API)
 * ------------------------------------------------------------------------- */

int Clic_getScreenWidth(void) {
    Clic_initPlatform();
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
        int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        return cols > 0 ? cols : 80;
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) {
        return 80;
    }
    return (int)w.ws_col;
#endif
}

int Clic_getScreenHeight(void) {
    Clic_initPlatform();
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
        int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return rows > 0 ? rows : 24;
    }
    return 24;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_row == 0) {
        return 24;
    }
    return (int)w.ws_row;
#endif
}

void Clic_fixDraw(void) {
    Clic_move(Clic_getScreenHeight() - 1, Clic_getScreenWidth());
    Clic_breakLine();
}

/* -------------------------------------------------------------------------
 * 6. Desenho de Simbolos, Linhas e Caixas (Molduras)
 * ------------------------------------------------------------------------- */

void Clic_printSymbol(const char *symbol) {
    Clic_initPlatform();
    if (symbol) {
        printf("%s", symbol);
    }
}

void Clic_printHLine(int width) {
    Clic_initPlatform();
    for (int i = 0; i < width; i++) {
        Clic_printSymbol(Symbol_HLINE);
    }
}

void Clic_printVLine(int height) {
    Clic_initPlatform();
    for (int i = 0; i < height; i++) {
        printf(Symbol_VLINE CLIC_ESC "[1D" CLIC_ESC "[1B");
    }
}

void Clic_printHBlockLine(int width) {
    Clic_initPlatform();
    for (int i = 0; i < width; i++) {
        printf(" ");
    }
}

void Clic_printVBlockLine(int height) {
    Clic_initPlatform();
    for (int i = 0; i < height; i++) {
        printf(" " CLIC_ESC "[1D" CLIC_ESC "[1B");
    }
}

/** Desenha a borda superior de uma caixa: ┏━━┓ */
static void Clic_printUpBorder(int width) {
    Clic_printSymbol(Symbol_CORNER_TL);
    Clic_printHLine(width - 2);
    Clic_printSymbol(Symbol_CORNER_TR);
}

/** Desenha a borda inferior de uma caixa: ┗━━┛ */
static void Clic_printBottomBorder(int width) {
    Clic_printSymbol(Symbol_CORNER_BL);
    Clic_printHLine(width - 2);
    Clic_printSymbol(Symbol_CORNER_BR);
}

void Clic_printBox(int width, int height) {
    Clic_initPlatform();
    if (width < 3 || height < 3) {
        return;
    }

    int screenW = Clic_getScreenWidth();
    int screenH = Clic_getScreenHeight();
    if (width >= screenW)  width = screenW;
    if (height >= screenH) height = screenH - 1;

    Clic_saveCursorPosition();
    Clic_printUpBorder(width);

    int i;
    for (i = 1; i <= height - 2; i++) {
        Clic_restoreCursorPosition();
        Clic_moveDown(i);
        printf(Symbol_VLINE);
        Clic_printHBlockLine(width - 2);
        printf(Symbol_VLINE);
    }

    Clic_restoreCursorPosition();
    Clic_moveDown(i);
    Clic_printBottomBorder(width);
}

int Clic_printCenter(const char *text) {
    Clic_initPlatform();
    int textWidth = Clic_textWidth(text);
    if (textWidth < 0) {
        return 0;
    }

    int screenWidth = Clic_getScreenWidth();
    if (screenWidth <= 0) {
        return 0;
    }

    int visibleWidth = textWidth < screenWidth ? textWidth : screenWidth;
    int column = 1 + (screenWidth - visibleWidth) / 2;
    Clic_moveToColumn(column);
    Text_printCell(text, visibleWidth, true);
    return column;
}

void Clic_printScreenBox(void) {
    int width = Clic_getScreenWidth();
    int height = Clic_getScreenHeight();

    Clic_clearScreen();
    Clic_printBox(width, height);
}

/* -------------------------------------------------------------------------
 * 7. Captura de Teclado e Temporizacao (Linux termios vs Windows conio.h)
 * ------------------------------------------------------------------------- */

#ifndef _WIN32
/**
 * No Linux, o terminal opera por padrao no "modo canonico" (ICANON), onde o
 * sistema operacional guarda as letras digitadas e so entrega ao programa C
 * quando o usuario aperta ENTER. Alem disso, o modo ECHO imprime cada tecla
 * automaticamente na tela. As funcoes abaixo ligam e desligam esses modos.
 */
static void Clic_setBufferOn(void) {
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) == 0) {
        t.c_lflag |= (tcflag_t)(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
}

static void Clic_setBufferOff(void) {
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) == 0) {
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
}

static void Clic_setNonBlockingInputOn(void) {
    Clic_setBufferOff();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
}

static void Clic_setNonBlockingInputOff(void) {
    Clic_setBufferOn();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }
}
#endif

#ifdef _WIN32
static int Clic_translateWinKey(int ch) {
    if (ch == 0 || ch == 224) {
        int ext = _getch();
        if (ext == 72) return Key_UP;
        if (ext == 80) return Key_DOWN;
        if (ext == 77) return Key_RIGHT;
        if (ext == 75) return Key_LEFT;
        if (ext == 83) return Key_DELETE;
        return ext;
    }
    if (ch == '\r') return Key_ENTER;
    if (ch == 8)    return Key_DELETE;
    return ch;
}
#endif

int Clic_keyCapture(void) {
    Clic_initPlatform();
    fflush(stdout);

#ifdef _WIN32
    return Clic_translateWinKey(_getch());
#else
    int keyPressed = 0;
    int codeKey = 0;

    Clic_setBufferOff();
    keyPressed = getchar();

    if (keyPressed == '\033') {
        /* Verifica se ha '[' imediatamente apos ESC (sequencia de seta ANSI) */
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags != -1) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        int next = getchar();
        if (next == '[') {
            if (flags != -1) fcntl(STDIN_FILENO, F_SETFL, flags);
            codeKey = getchar();
        } else {
            if (flags != -1) fcntl(STDIN_FILENO, F_SETFL, flags);
            codeKey = Key_ESC;
        }
    } else {
        codeKey = keyPressed;
    }

    Clic_setBufferOn();
    return codeKey;
#endif
}

int Clic_keyCaptureNonBlocking(void) {
    Clic_initPlatform();
    fflush(stdout);

#ifdef _WIN32
    if (!_kbhit()) {
        return EOF;
    }
    return Clic_translateWinKey(_getch());
#else
    int keyPressed = 0;
    int codeKey = EOF;

    Clic_setNonBlockingInputOn();
    keyPressed = getchar();

    if (keyPressed == '\033') {
        int next = getchar();
        if (next == '[') {
            codeKey = getchar();
        } else {
            codeKey = Key_ESC;
        }
    } else {
        codeKey = keyPressed;
    }

    Clic_setNonBlockingInputOff();
    return codeKey;
#endif
}

void Clic_pause(float seconds) {
    fflush(stdout);
    if (seconds <= 0.0f) {
        return;
    }
#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000.0f));
#else
    struct timespec req;
    req.tv_sec  = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (float)req.tv_sec) * 1000000000.0f);
    nanosleep(&req, NULL);
#endif
}

/* -------------------------------------------------------------------------
 * 8. Tabelas Formatadas e Selecao Interativa
 * ------------------------------------------------------------------------- */

static int Format_extractWidth(const char *format) {
    const char *p = format;
    while (*p != '\0') {
        if (*p >= '0' && *p <= '9') {
            return atoi(p);
        }
        p++;
    }
    return 0;
}

static bool Format_isTypeSpecifier(char c) {
    return strchr("diuoxXfFeEgGaAcspn%", c) != NULL;
}

static Format *Format_formatSplit(const char *sourceString) {
    const char *p = sourceString;
    const char *startOfFormat = NULL;
    int count = 0;

    Format *formats = calloc(Row_MAX_CELLS, sizeof(Format));
    if (!formats) {
        return NULL;
    }

    while (*p != '\0') {
        if (*p == '%' && startOfFormat == NULL) {
            startOfFormat = p;
        } else if (startOfFormat != NULL) {
            if (Format_isTypeSpecifier(*p)) {
                size_t len = (size_t)(p - startOfFormat) + 1;

                if (count < Row_MAX_CELLS && len < sizeof(formats[count].formatString)) {
                    memcpy(formats[count].formatString, startOfFormat, len);
                    formats[count].formatString[len] = '\0';
                    formats[count].isLeftAligned =
                        (strchr(formats[count].formatString, '-') != NULL);
                    count++;
                }

                if (startOfFormat[1] == '%') {
                    p++;
                }
                startOfFormat = NULL;
            } else if (*p >= '0' && *p <= '9') {
                formats[count].isCustomWidth = true;
                formats[count].width = Format_extractWidth(startOfFormat);
            } else if (strchr("-+ #.*hlLzjt", *p) == NULL) {
                startOfFormat = NULL;
            }
        }

        if (*p == '\0') {
            break;
        }
        p++;
    }

    return formats;
}

Table *Table_create(const char *format) {
    Clic_initPlatform();

    if (format == NULL) {
        fprintf(stderr, "Erro em Table_create: string de formato nula.\n");
        return NULL;
    }

    Table *table = calloc(1, sizeof(Table));
    if (!table) {
        fprintf(stderr, "Erro em Table_create: falha ao alocar memoria.\n");
        return NULL;
    }

    table->formats = Format_formatSplit(format);
    if (!table->formats) {
        free(table);
        return NULL;
    }

    int countFormats = 0;
    while (countFormats < Row_MAX_CELLS &&
           table->formats[countFormats].formatString[0] != '\0') {
        countFormats++;
    }

    if (countFormats <= 0) {
        fprintf(stderr, "Erro em Table_create: nenhum formato valido encontrado.\n");
        free(table->formats);
        free(table);
        return NULL;
    }

    int width = Clic_getScreenWidth();
    table->firstSelectableIndex = 1;
    table->highlightedIndex = 1;
    table->highlightColor = Color_YELLOW;
    table->nCols = countFormats;
    table->nRows = 0;
    table->width = width;
    table->defaultCellWidth = (width - 2) / table->nCols;
    table->firstRow = NULL;
    table->lastRow = NULL;

    return table;
}

void Table_setWidth(Table *table, int width) {
    if (table == NULL) {
        return;
    }
    int screenW = Clic_getScreenWidth();
    if (width >= screenW) {
        width = screenW;
    }
    if (width < 3) {
        width = 3;
    }

    table->width = width;
    table->defaultCellWidth = (width - 2) / table->nCols;
}

static void Table_printHighlight(Table *table, bool highlight) {
    if (!table) {
        return;
    }

    int width = table->width;
    if (table->nCols <= 0 || table->nCols > Row_MAX_CELLS ||
        width < 2 + table->nCols) {
        return;
    }

    int cellWidths[Row_MAX_CELLS] = {0};
    int remainingWidth = width - (2 + table->nCols);
    int defaultCellCount = 0;

    for (int col = 0; col < table->nCols; col++) {
        if (table->formats[col].isCustomWidth) {
            int requested = table->formats[col].width;
            if (requested < 0) requested = 0;
            cellWidths[col] = requested < remainingWidth ? requested : remainingWidth;
            remainingWidth -= cellWidths[col];
        } else {
            defaultCellCount++;
        }
    }

    int defaultCellWidth = defaultCellCount > 0 ? remainingWidth / defaultCellCount : 0;
    for (int col = 0; col < table->nCols; col++) {
        if (!table->formats[col].isCustomWidth) {
            cellWidths[col] = defaultCellWidth;
            remainingWidth -= defaultCellWidth;
        }
    }
    cellWidths[table->nCols - 1] += remainingWidth;

    Row *currentRow = table->firstRow;

    Clic_saveCursorPosition();
    Clic_printUpBorder(width);

    int linesDown = 1;
    Clic_restoreCursorPosition();
    Clic_moveDown(linesDown);

    while (currentRow != NULL) {
        Clic_printSymbol(Symbol_VLINE);

        for (int col = 0; col < table->nCols; col++) {
            Cell cell = currentRow->cell[col];
            int cellWidth = cellWidths[col];

            if (highlight && table->highlightedIndex == currentRow->index) {
                Clic_setBackgroundColor(table->highlightColor);
                Clic_setFontColor(Color_BLACK);
            }

            Text_printCell(cell.content, cellWidth, cell.format.isLeftAligned);
            printf(" ");

            Clic_resetColor();
        }

        Clic_printSymbol(Symbol_VLINE);
        Clic_restoreCursorPosition();
        Clic_moveDown(++linesDown);

        currentRow = currentRow->next;

        if (currentRow) {
            Clic_printSymbol(Symbol_VLINE);
            Clic_printHLine(width - 2);
            Clic_printSymbol(Symbol_VLINE);
            Clic_restoreCursorPosition();
            Clic_moveDown(++linesDown);
        }
    }

    Clic_printBottomBorder(width);

    if (highlight) {
        Clic_restoreCursorPosition();
    }
}

void Table_print(Table *table) {
    Table_printHighlight(table, false);
}

int Table_select(Table *table) {
    if (!table || table->nRows <= 0) {
        return -1;
    }

    if (table->firstSelectableIndex > 1) {
        if (table->nRows < table->firstSelectableIndex) {
            return -1;
        }
        if (table->highlightedIndex < table->firstSelectableIndex) {
            table->highlightedIndex = table->firstSelectableIndex;
        }
    }

    int key = 0;
    do {
        Table_printHighlight(table, true);
        key = Clic_keyCapture();

        switch (key) {
            case Key_UP:
                if (table->highlightedIndex > 1 &&
                    (table->firstSelectableIndex <= 1 ||
                     table->highlightedIndex > table->firstSelectableIndex)) {
                    table->highlightedIndex--;
                }
                break;
            case Key_DOWN:
                if (table->highlightedIndex < table->nRows) {
                    table->highlightedIndex++;
                }
                break;
            default:
                break;
        }

        if (key == Key_DELETE) {
            return (-1) * table->highlightedIndex;
        }
        if (key == Key_ESC) {
            return -1;
        }
    } while (key != Key_ENTER);

    return table->highlightedIndex;
}

static void Row_destroy(Row *row) {
    if (!row) {
        return;
    }
    for (int col = 0; col < row->length; col++) {
        free(row->cell[col].content);
    }
    free(row);
}

static char *Cell_formatContent(const char *format, ...) {
    va_list args, sizeArgs;
    va_start(args, format);
    va_copy(sizeArgs, args);
    int length = vsnprintf(NULL, 0, format, sizeArgs);
    va_end(sizeArgs);

    if (length < 0) {
        va_end(args);
        return NULL;
    }

    size_t capacity = (size_t)length + 1;
    char *content = malloc(capacity);
    if (content) {
        int written = vsnprintf(content, capacity, format, args);
        if (written < 0 || (size_t)written >= capacity || Clic_textWidth(content) < 0) {
            free(content);
            content = NULL;
        }
    }

    va_end(args);
    return content;
}

void Table_free(Table *table) {
    if (!table) {
        return;
    }

    Row *currentRow = table->firstRow;
    while (currentRow != NULL) {
        Row *nextRow = currentRow->next;
        Row_destroy(currentRow);
        currentRow = nextRow;
    }

    free(table->formats);
    free(table);
}

static Row *Row_createFromVaList(Table *table, va_list *args, bool forceStrings) {
    if (table == NULL) {
        return NULL;
    }

    Row *newRow = calloc(1, sizeof(Row));
    if (!newRow) {
        return NULL;
    }

    for (int col = 0; col < table->nCols; col++) {
        char *formatString = table->formats[col].formatString;
        char *buffer = NULL;

        if (forceStrings || strchr(formatString, 's') != NULL) {
            char *strArg = va_arg(*args, char *);
            char stringFormat[sizeof(table->formats[col].formatString)];
            const char *precision = strchr(formatString, '.');
            if (!forceStrings && precision) {
                snprintf(stringFormat, sizeof(stringFormat), "%%%s", precision);
            } else {
                strcpy(stringFormat, "%s");
            }
            if (strArg) {
                buffer = Cell_formatContent(stringFormat, strArg);
            }
        } else if (strpbrk(formatString, "*hlLzjt") == NULL) {
            if (strchr(formatString, 'd') || strchr(formatString, 'i')) {
                int intArg = va_arg(*args, int);
                buffer = Cell_formatContent(formatString, intArg);
            } else if (strchr(formatString, 'f') || strchr(formatString, 'F')) {
                double doubleArg = va_arg(*args, double);
                buffer = Cell_formatContent(formatString, doubleArg);
            } else if (strchr(formatString, 'c')) {
                int charArg = va_arg(*args, int);
                buffer = Cell_formatContent(formatString, charArg);
            }
        }

        if (!buffer) {
            fprintf(stderr, "Erro em Table: falha ao formatar coluna %d.\n", col + 1);
            Row_destroy(newRow);
            return NULL;
        }

        newRow->cell[col].content = buffer;
        newRow->cell[col].format  = table->formats[col];
        newRow->length++;
    }

    return newRow;
}

static void Table_appendRowNode(Table *table, Row *newRow) {
    table->nRows++;
    newRow->index  = table->nRows;
    newRow->length = table->nCols;
    newRow->next   = NULL;
    newRow->prev   = table->lastRow;

    if (table->lastRow) {
        table->lastRow->next = newRow;
    } else {
        table->firstRow = newRow;
    }
    table->lastRow = newRow;
}

void Table_setHeader(Table *table, ...) {
    if (table == NULL) {
        return;
    }

    va_list args;
    va_start(args, table);
    /* No cabecalho, todas as colunas recebem titulos em texto (char*) */
    Row *headerRow = Row_createFromVaList(table, &args, true);
    va_end(args);

    if (!headerRow) {
        return;
    }

    Table_appendRowNode(table, headerRow);
    table->firstSelectableIndex = 2;
    if (table->highlightedIndex < 2) {
        table->highlightedIndex = 2;
    }
}

void Table_addRow(Table *table, ...) {
    if (table == NULL) {
        return;
    }

    va_list args;
    va_start(args, table);
    Row *newRow = Row_createFromVaList(table, &args, false);
    va_end(args);

    if (!newRow) {
        return;
    }

    Table_appendRowNode(table, newRow);
}