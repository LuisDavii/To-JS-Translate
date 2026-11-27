%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

int yylex(void);
extern int yylineno;

Node *program_root = NULL;
%}

/* Incluido tambem no parser.tab.h, para que o tipo Node exista onde o %union for usado (ex.: no lexer). */
%code requires {
#include "ast.h"
}

/* Habilita rastreio de posicao: @1, @2... fornecem a linha de cada simbolo (usado nos erros). */
%locations
/* Mensagens de erro sintatico detalhadas ("unexpected X, expecting Y"). */
%define parse.error verbose

/*
 * Tipo do valor semantico de cada simbolo:
 *   text -> string (lexemas de identificador, numero e string, vindos do lexer)
 *   node -> ponteiro para no da AST (nao-terminais da gramatica)
 */
%union {
    char *text;
    Node *node;
}

/* Em caso de erro sintatico, o Bison libera os valores que ficaram na pilha (evita vazamento de memoria). */
%destructor { free($$); } <text>
%destructor { ast_free($$); } <node>

/* Tokens que carregam texto (lexema). Os demais tokens abaixo nao carregam valor. */
%token <text> IDENT STRING
%token <text> INTEGER REAL

/* Palavras-chave e literais (True, False, None; if, else, while, print; and, or, not). */
%token TRUE FALSE NONE
%token IF ELSE WHILE PRINT
%token AND OR NOT

/* Operadores de comparacao, atribuicao (=) e aritmeticos. */
%token EQ NE GE LE GT LT ASSIGN
%token PLUS MINUS STAR SLASH MOD

/* Pontuacao. FIM_COMANDO e o `;`. (COMMA esta declarado mas ainda nao e usado na gramatica.) */
%token LPAREN RPAREN COLON COMMA FIM_COMANDO
/*
 * Tokens "virtuais" gerados pelo lexer (nao existem como texto): fim de linha e abertura/fechamento
 * de bloco por indentacao. Equivalem as chaves { } de outras linguagens.
 */
%token NEWLINE INDENT DEDENT

/* Declara que estes nao-terminais produzem `Node *` (campo `node` do %union). */
%type <node> programa comandos comando
%type <node> comando_simples comando_composto
%type <node> atribuicao saida condicional repeticao
%type <node> expressao expressao_or expressao_and
%type <node> expressao_not comparacao
%type <node> expressao_aritmetica termo fator primario

/*
 * Precedencia e associatividade, da MENOR (topo) para a MAIOR (final):
 *   or < and < not < comparacoes < + - < * / % < unarios.
 * `%left` = associa a esquerda; `%right` = a direita; `%nonassoc` = nao encadeia.
 * A hierarquia de nao-terminais da gramatica (expressao_or -> expressao_and -> ...)
 * ja impoe essa ordem; as declaracoes reforcam e documentam as regras.
 */
%left OR
%left AND
%right NOT

/* Comparacoes nao encadeiam: `1 < 2 < 3` e erro sintatico (diferente do Python). */
%nonassoc EQ NE GT LT GE LE

%left PLUS MINUS
%left STAR SLASH MOD

/* Tokens fantasmas, usados so com %prec para dar precedencia aos unarios +x e -x. */
%right UPLUS UMINUS

/* Simbolo inicial da gramatica. */
%start programa

%%

/*
 * Regra inicial: um programa e uma lista de comandos. Embrulha a lista no no raiz (N_LIST)
 * e guarda em `program_root`, que o main.c usa depois.
 */
programa:
    comandos
    {
        program_root = node_list($1, @1.first_line);
        $$ = NULL;
    }
    ;

/*
 * Lista de comandos, recursiva a ESQUERDA (eficiente no LALR):
 * vazio, ou (lista ja pronta + mais um comando). node_append encadeia via `next`.
 */
comandos:
      /* vazio */
      {
          $$ = NULL;
      }

    | comandos comando
      {
          $$ = node_append($1, $2);
      }
    ;

/*
 * Um comando pode ser:
 *   - simples (atribuicao ou print), obrigatoriamente terminado em `;` + fim de linha
 *   - composto (if/while), cujo fim e dado pelo DEDENT do bloco
 *   - linha em branco (so NEWLINE) -> NULL, ignorada pela AST
 */
comando:
      comando_simples FIM_COMANDO NEWLINE
      {
          $$ = $1;
      }

    | comando_composto
      {
          $$ = $1;
      }

    | NEWLINE
      {
          $$ = NULL;
      }
    ;

/* Comandos de uma linha. */
comando_simples:
      atribuicao
      {
          $$ = $1;
      }

    | saida
      {
          $$ = $1;
      }
    ;

/* Comandos com bloco indentado. */
comando_composto:
      condicional
      {
          $$ = $1;
      }

    | repeticao
      {
          $$ = $1;
      }
    ;

/* Atribuicao */

/*
 * `x = expr`. Cria N_ASSIGN com o nome em `text` e a expressao em `left`.
 * free($1) libera o texto do lexer, ja que node_text faz sua propria copia.
 */
atribuicao:
    IDENT ASSIGN expressao
    {
        $$ = node_text(N_ASSIGN, $1, @1.first_line);
        $$->left = $3;
        free($1);
    }
    ;

/* Saida */

/* `print(expr)` -> N_PRINT. Os parenteses sao obrigatorios (parte da gramatica). */
saida:
    PRINT LPAREN expressao RPAREN
    {
        $$ = node_new(N_PRINT, @1.first_line);
        $$->left = $3;
    }
    ;

/* Estrutura condicional */

/*
 * `if cond:` + NEWLINE + bloco indentado (INDENT ... DEDENT), com `else:` opcional.
 * Numeracao dos simbolos na acao: $2 = condicao, $6 = comandos do then; na 2a forma,
 * $12 = comandos do else.
 */
condicional:
      IF expressao COLON NEWLINE
      INDENT comandos DEDENT
      {
          $$ = node_new(N_IF, @1.first_line);
          $$->left = $2;
          $$->body = $6;
          $$->else_body = NULL;
      }

    | IF expressao COLON NEWLINE
      INDENT comandos DEDENT
      ELSE COLON NEWLINE
      INDENT comandos DEDENT
      {
          $$ = node_new(N_IF, @1.first_line);
          $$->left = $2;
          $$->body = $6;
          $$->else_body = $12;
      }
    ;

/* Estrutura de repeticao */

/* `while cond:` + bloco indentado. $2 = condicao, $6 = corpo. */
repeticao:
    WHILE expressao COLON NEWLINE
    INDENT comandos DEDENT
    {
        $$ = node_new(N_WHILE, @1.first_line);
        $$->left = $2;
        $$->body = $6;
    }
    ;

/* Expressoes */

/*
 * Cadeia de nao-terminais, do mais fraco (or) ao mais forte (primario).
 * Quanto mais fundo na cadeia, MAIOR a precedencia:
 *   expressao -> or -> and -> not -> comparacao -> aritmetica (+ -) -> termo (* / %) -> fator (unario) -> primario
 */
expressao:
    expressao_or
    {
        $$ = $1;
    }
    ;

/* `a or b` (associa a esquerda). Cria N_BINARY com operador "or". */
expressao_or:
      expressao_or OR expressao_and
      {
          $$ = node_binary("or", $1, $3, @2.first_line);
      }

    | expressao_and
      {
          $$ = $1;
      }
    ;

/* `a and b` (associa a esquerda). */
expressao_and:
      expressao_and AND expressao_not
      {
          $$ = node_binary("and", $1, $3, @2.first_line);
      }

    | expressao_not
      {
          $$ = $1;
      }
    ;

/* `not x` (recursivo, associa a direita) ou desce para comparacao. */
expressao_not:
      NOT expressao_not
      {
          $$ = node_unary("not", $2, @1.first_line);
      }

    | comparacao
      {
          $$ = $1;
      }
    ;

/*
 * No maximo UMA comparacao por vez (ha so um operador entre duas expressoes aritmeticas),
 * por isso `1 < 2 < 3` nao e aceito.
 */
comparacao:
      expressao_aritmetica
      {
          $$ = $1;
      }

    | expressao_aritmetica EQ expressao_aritmetica
      {
          $$ = node_binary("==", $1, $3, @2.first_line);
      }

    | expressao_aritmetica NE expressao_aritmetica
      {
          $$ = node_binary("!=", $1, $3, @2.first_line);
      }

    | expressao_aritmetica GT expressao_aritmetica
      {
          $$ = node_binary(">", $1, $3, @2.first_line);
      }

    | expressao_aritmetica LT expressao_aritmetica
      {
          $$ = node_binary("<", $1, $3, @2.first_line);
      }

    | expressao_aritmetica GE expressao_aritmetica
      {
          $$ = node_binary(">=", $1, $3, @2.first_line);
      }

    | expressao_aritmetica LE expressao_aritmetica
      {
          $$ = node_binary("<=", $1, $3, @2.first_line);
      }
    ;

/* Operacoes aritmeticas */

/* Soma e subtracao (menor precedencia aritmetica), associativas a esquerda. */
expressao_aritmetica:
      expressao_aritmetica PLUS termo
      {
          $$ = node_binary("+", $1, $3, @2.first_line);
      }

    | expressao_aritmetica MINUS termo
      {
          $$ = node_binary("-", $1, $3, @2.first_line);
      }

    | termo
      {
          $$ = $1;
      }
    ;

/* Multiplicacao, divisao e modulo; ligam mais forte que + e -. */
termo:
      termo STAR fator
      {
          $$ = node_binary("*", $1, $3, @2.first_line);
      }

    | termo SLASH fator
      {
          $$ = node_binary("/", $1, $3, @2.first_line);
      }

    | termo MOD fator
      {
          $$ = node_binary("%", $1, $3, @2.first_line);
      }

    | fator
      {
          $$ = $1;
      }
    ;

/* Sinal unario (+x, -x); %prec define a precedencia da regra. Sem sinal, desce para primario. */
fator:
      PLUS fator %prec UPLUS
      {
          $$ = node_unary("+", $2, @1.first_line);
      }

    | MINUS fator %prec UMINUS
      {
          $$ = node_unary("-", $2, @1.first_line);
      }

    | primario
      {
          $$ = $1;
      }
    ;

/* Valores primarios */

/*
 * Elementos indivisiveis: variavel, literais, ou expressao entre parenteses.
 * Os parenteses NAO viram no da AST (a estrutura da arvore ja carrega a ordem).
 */
primario:
      IDENT
      {
          $$ = node_text(N_ID, $1, @1.first_line);
          free($1);
      }

    | INTEGER
      {
          $$ = node_number(N_INT, 0.0, $1, @1.first_line);
          free($1);
      }

    | REAL
      {
          $$ = node_number(N_REAL, 0.0, $1, @1.first_line);
          free($1);
      }

    | STRING
      {
          $$ = node_text(N_STRING, $1, @1.first_line);
          free($1);
      }

    | TRUE
      {
          $$ = node_bool(1, @1.first_line);
      }

    | FALSE
      {
          $$ = node_bool(0, @1.first_line);
      }

    | NONE
      {
          $$ = node_text(N_NONE, "null", @1.first_line);
      }

    | LPAREN expressao RPAREN
      {
          $$ = $2;
      }
    ;

%%

/* Chamada pelo Bison em erro sintatico: imprime a linha e a mensagem detalhada no stderr. */
void yyerror(const char *message) {
    int line = yylloc.first_line > 0 ? yylloc.first_line : yylineno;
    fprintf(stderr, "Erro sintatico na linha %d: %s\n", line, message);
}
