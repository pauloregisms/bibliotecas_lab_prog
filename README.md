# Bibliotecas Didáticas `Clic` e `Cnet` — Guia Rápido do Estudante

Bem-vindo ao guia de referência rápida das bibliotecas **`Clic`** (Interface de Terminal e Tabelas Interativas) e **`Cnet`** (Comunicação em Rede TCP/IPv4 e Descoberta Automática UDP).

As duas bibliotecas foram projetadas para as aulas de **Laboratório de Programação em C**:
* **Independentes:** Você pode usar apenas a `Clic.h` para criar jogos e painéis visuais, apenas a `Cnet.h` para criar servidores e clientes de rede, ou juntar as duas no mesmo projeto.
* **Multiplataforma (Linux e Windows):** O mesmo código-fonte compila e roda nos computadores Linux do laboratório e no Windows de casa (usando MinGW / GCC no VS Code, Code::Blocks ou Dev-C++).

## 1. Como Compilar seus Programas

Coloque os arquivos da biblioteca na mesma pasta do seu programa `.c` e execute no terminal:

| O que seu programa usa | Comando no Linux / macOS | Comando no Windows (MinGW / GCC) |
| :--- | :--- | :--- |
| **Apenas `Clic`** (`#include "Clic.h"`) | `gcc programa.c Clic.c -o programa` | `gcc programa.c Clic.c -o programa.exe` |
| **Apenas `Cnet`** (`#include "Cnet.h"`) | `gcc programa.c Cnet.c -o programa` | `gcc programa.c Cnet.c -lws2_32 -o programa.exe` |
| **`Clic` + `Cnet` juntas** | `gcc programa.c Clic.c Cnet.c -o programa` | `gcc programa.c Clic.c Cnet.c -lws2_32 -o programa.exe` |

> **Nota para usuários Windows:** O parâmetro `-lws2_32` no final do comando ativa a biblioteca oficial de sockets de rede do Windows (`Winsock2`). A biblioteca `Cnet` cuida de toda a inicialização do Windows automaticamente para você.

## 2. As 3 Regras de Ouro (Evite Erros Comuns!)

1. **Terminal com suporte a UTF-8:** As bordas das caixas e tabelas usam caracteres Unicode (`┏━━┓`). No Windows, execute seus programas no **Windows Terminal** ou no terminal integrado do **VS Code**.
2. **Crie sempre um vetor `char msg[Client_MESSAGE_SIZE]` para receber mensagens:** Nunca passe um ponteiro solto sem memória alocada (`char *msg;`) para `Client_receiveLine` ou `Server_receiveLine`.
3. **Quem cria, fecha:**
   * Criou uma tabela com `Table_create(...)`? Libere no final com `Table_free(tabela)`.
   * Conectou como cliente com `Client_connect(...)` ou `Client_connectInfo(...)`? Feche com `Client_close(cli)`.
   * **Atenção:** Nunca chame `Client_close` ou `Client_free` em um ponteiro `Client*` devolvido por `Server_receiveLine`! Os clientes conectados pertencem ao `Server` e são liberados automaticamente por `Server_close(servidor)`.

## 3. Referência Rápida — Biblioteca `Clic` (`#include "Clic.h"`)

### Cores, Tela e Cursor

| Função | Descrição Prática |
| :--- | :--- |
| `Clic_clearScreen()` | Apaga toda a tela e posiciona o cursor no canto superior esquerdo `(1, 1)`. |
| `Clic_setFontColor(cor)` | Muda a cor da letra (`Color_YELLOW`, `Color_GREEN`, `Color_CYAN`, `Color_RED`, `Color_ORANGE`, `Color_WHITE`, `Color_BLUE`, `Color_MAGENTA`). |
| `Clic_setBackgroundColor(cor)` | Muda a cor de fundo dos próximos textos impressos. |
| `Clic_resetColor()` | Restaura as cores padrão do terminal (chame sempre após imprimir colorido). |
| `Clic_move(linha, coluna)` | Move o cursor para a coordenada `(linha, coluna)`, começando em `(1, 1)`. |
| `Clic_hideCursor()` / `Clic_showCursor()` | Esconde ou mostra o cursor piscante do terminal. |
| `Clic_fixDraw()` | Posiciona o cursor no rodapé da tela antes do `return 0;` para não sobrepor seu desenho. |
| `Clic_getScreenWidth()` / `Clic_getScreenHeight()` | Retorna a largura (colunas) e a altura (linhas) atuais da janela do terminal. |

### Molduras, Teclado e Tabelas Interativas

| Função | Descrição Prática |
| :--- | :--- |
| `Clic_printBox(largura, altura)` | Desenha uma caixa com bordas `┏━━┓` a partir da posição atual do cursor. |
| `Clic_printScreenBox()` | Limpa a tela e desenha uma moldura do tamanho exato da janela do terminal. |
| `Clic_printCenter("Texto")` | Imprime um texto centralizado horizontalmente na linha atual. |
| `Clic_keyCapture()` | Espera o usuário apertar uma tecla e retorna seu código (`Key_UP`, `Key_DOWN`, `Key_LEFT`, `Key_RIGHT`, `Key_ENTER`, `Key_DELETE`, `Key_ESC`). |
| `Clic_keyCaptureNonBlocking()` | Verifica se uma tecla foi pressionada **sem travar o programa** (retorna `EOF` se nenhuma tecla foi apertada). |
| `Clic_pause(segundos)` | Pausa o programa pelo tempo em segundos (ex: `0.5f` para meio segundo, `0.05f` para 50 ms). |
| `Table_create("%-20s %6d %8.2f")` | Cria uma tabela definindo o formato de cada coluna no padrão `printf`. |
| `Table_setHeader(tabela, ...)` | Adiciona uma linha de título no topo da tabela e faz `Table_select` pular o cabeçalho automaticamente. |
| `Table_addRow(tabela, ...)` | Adiciona uma linha de dados respeitando os tipos de cada coluna. |
| `Table_print(tabela)` | Desenha a tabela na tela sem abrir menu de seleção. |
| `Table_select(tabela)` | Permite navegar pelas linhas com as setas `↑` `↓`. Retorna o número da linha ao teclar `ENTER`, o número negativo ao teclar `DELETE`, ou `-1` ao teclar `ESC`. |
| `Table_free(tabela)` | Libera toda a memória da tabela. |

## 4. Referência Rápida — Biblioteca `Cnet` (`#include "Cnet.h"`)

### Funções do Cliente TCP e Descoberta na Rede Local

| Função | Descrição Prática |
| :--- | :--- |
| `Network_discoverServers(lista, max)` | Procura servidores ativos na rede local por 1 segundo e preenche o vetor `ServerInfo lista[]`, retornando quantos encontrou. |
| `Client_connect("127.0.0.1", 5000)` | Conecta a um servidor pelo endereço IP e porta TCP. Retorna `Client*` ou `NULL`. |
| `Client_connectInfo(lista[i])` | Conecta diretamente a um servidor descoberto no vetor `ServerInfo`. |
| `Client_sendLine(cli, "Ola!")` | Envia uma mensagem de texto garantindo o caractere `\n` no final. |
| `Client_receiveLine(cli, buffer)` | Aguarda chegar uma linha completa e salva em `buffer` (sem o `\n`). |
| `Client_receiveLineNonBlocking(cli, buffer)` | Verifica se chegou uma linha sem travar o programa (retorna `Client_NO_DATA` se vazio). |
| `Client_close(cli)` / `Client_free(cli)` | Fecha a conexão e libera a memória do cliente. |

### Funções do Servidor TCP

| Função | Descrição Prática |
| :--- | :--- |
| `Server_create(5000)` | Cria um servidor TCP escutando na porta indicada. |
| `Server_setName(srv, "Sala 01")` | Define o nome público do servidor para aparecer nas buscas da rede local. |
| `Server_enableDiscovery(srv)` | Ativa a resposta automática a buscas de alunos na rede local (porta UDP 39000). |
| `Server_receiveLine(srv, buffer)` | Aguarda uma linha de **qualquer** cliente conectado (e aceita novos clientes automaticamente enquanto espera). Retorna o `Client*` que enviou a mensagem. |
| `Server_receiveLineNonBlocking(srv, buffer)` | Verifica se algum cliente enviou uma linha sem travar o programa (retorna `NULL` se ninguém enviou nada). |
| `Server_sendAll(srv, "Aviso\n")` | Envia uma mensagem para todos os clientes conectados ao mesmo tempo. |
| `Server_sendOthers(srv, autor, "Msg\n")` | Envia uma mensagem para todos os clientes, exceto quem enviou (`autor`). |
| `Server_close(srv)` | Desconecta todos os clientes, fecha o servidor e libera toda a memória. |

## 5. Arquivos de Exemplo e Mini-Projeto Incluídos

1. **`ex01_painel.c`** (`Clic.h`): Como desenhar molduras, posicionar o cursor e usar cores e símbolos.
2. **`ex02_tabela.c`** (`Clic.h`): Como criar uma pauta de alunos com `Table_setHeader` e selecionar linhas com as setas.
3. **`ex03_servidor.c`** (`Cnet.h`): Como criar um servidor TCP com descoberta automática na rede local sem depender de `Clic.h`.
4. **`ex04_cliente.c`** (`Cnet.h`): Como encontrar servidores com `Network_discoverServers(lista, 10)` e trocar mensagens com o servidor.
5. **Mini-Projeto Integrador (`professor_servidor.c` + `aluno_cliente.c`)**: Sistema completo de **Fila de Dúvidas do Laboratório em Rede** unindo a descoberta UDP e comunicação TCP da `Cnet` com as tabelas interativas e painéis em tempo real da `Clic`.
