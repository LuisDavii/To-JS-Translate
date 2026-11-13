%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

int yylex(void);
extern int yylineno;

Node *program_root = NULL;
%}

%code requires {
#include "ast.h"
}

%locations
%define parse.error verbose

%union {
    char *text;
    Node *node;
}

%destructor { free($$); } <text>
%destructor { ast_free($$); } <node>

%token <text> IDENT STRING
%token <text> INTEGER REAL

%token TRUE FALSE NONE
%token IF ELSE WHILE PRINT
%token AND OR NOT

%token EQ NE GE LE GT LT ASSIGN
%token PLUS MINUS STAR SLASH MOD

%token LPAREN RPAREN COLON COMMA FIM_COMANDO
%token NEWLINE INDENT DEDENT

%type <node> programa comandos comando
%type <node> comando_simples comando_composto
%type <node> atribuicao saida condicional repeticao
%type <node> expressao expressao_or expressao_and
%type <node> expressao_not comparacao
%type <node> expressao_aritmetica termo fator primario

%left OR
%left AND
%right NOT

%nonassoc EQ NE GT LT GE LE

%left PLUS MINUS
%left STAR SLASH MOD

%right UPLUS UMINUS

%start programa

%%

programa:
    comandos
    {
        program_root = node_list($1, @1.first_line);
        $$ = NULL;
    }
    ;

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

atribuicao:
    IDENT ASSIGN expressao
    {
        $$ = node_text(N_ASSIGN, $1, @1.first_line);
        $$->left = $3;
        free($1);
    }
    ;

/* Saida */

saida:
    PRINT LPAREN expressao RPAREN
    {
        $$ = node_new(N_PRINT, @1.first_line);
        $$->left = $3;
    }
    ;

/* Estrutura condicional */

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

expressao:
    expressao_or
    {
        $$ = $1;
    }
    ;

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

void yyerror(const char *message) {
    int line = yylloc.first_line > 0 ? yylloc.first_line : yylineno;
    fprintf(stderr, "Erro sintatico na linha %d: %s\n", line, message);
}
