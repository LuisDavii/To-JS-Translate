
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

%union {
    char *text;
    Node *node;
}

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
        program_root = $1;
        $$ = $1;
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
        $$ = node_new(N_ASSIGN);
        $$->text = $1;
        $$->left = $3;
    }
    ;

/* Saida */

saida:
    PRINT LPAREN expressao RPAREN
    {
        $$ = node_new(N_PRINT);
        $$->left = $3;
    }
    ;

/* Estrutura condicional */

condicional
    : IF expressao COLON NEWLINE
      INDENT comandos DEDENT
      {
          $$ = node_new(N_IF);
          $$->left = $2;
          $$->body = $6;
          $$->else_body = NULL;
      }
    | IF expressao COLON NEWLINE
      INDENT comandos DEDENT
      ELSE COLON NEWLINE
      INDENT comandos DEDENT
      {
          $$ = node_new(N_IF);
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
        $$ = node_new(N_WHILE);
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
          $$ = node_binary("or", $1, $3);
      }

    | expressao_and
      {
          $$ = $1;
      }
    ;

expressao_and:
      expressao_and AND expressao_not
      {
          $$ = node_binary("and", $1, $3);
      }

    | expressao_not
      {
          $$ = $1;
      }
    ;

expressao_not:
      NOT expressao_not
      {
          $$ = node_unary("not", $2);
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
          $$ = node_binary("==", $1, $3);
      }

    | expressao_aritmetica NE expressao_aritmetica
      {
          $$ = node_binary("!=", $1, $3);
      }

    | expressao_aritmetica GT expressao_aritmetica
      {
          $$ = node_binary(">", $1, $3);
      }

    | expressao_aritmetica LT expressao_aritmetica
      {
          $$ = node_binary("<", $1, $3);
      }

    | expressao_aritmetica GE expressao_aritmetica
      {
          $$ = node_binary(">=", $1, $3);
      }

    | expressao_aritmetica LE expressao_aritmetica
      {
          $$ = node_binary("<=", $1, $3);
      }
    ;

/* Operacoes aritmeticas */

expressao_aritmetica:
      expressao_aritmetica PLUS termo
      {
          $$ = node_binary("+", $1, $3);
      }

    | expressao_aritmetica MINUS termo
      {
          $$ = node_binary("-", $1, $3);
      }

    | termo
      {
          $$ = $1;
      }
    ;

termo:
      termo STAR fator
      {
          $$ = node_binary("*", $1, $3);
      }

    | termo SLASH fator
      {
          $$ = node_binary("/", $1, $3);
      }

    | termo MOD fator
      {
          $$ = node_binary("%", $1, $3);
      }

    | fator
      {
          $$ = $1;
      }
    ;

fator:
      PLUS fator %prec UPLUS
      {
          $$ = node_unary("+", $2);
      }

    | MINUS fator %prec UMINUS
      {
          $$ = node_unary("-", $2);
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
          $$ = node_text(N_ID, $1);
      }

    | INTEGER
      {
          $$ = node_text(N_INT, $1);
      }

    | REAL
      {
          $$ = node_text(N_REAL, $1);
      }

    | STRING
      {
          $$ = node_text(N_STRING, $1);
      }

    | TRUE
      {
          $$ = node_text(N_BOOL, "true");
      }

    | FALSE
      {
          $$ = node_text(N_BOOL, "false");
      }

    | NONE
      {
          $$ = node_text(N_NONE, "null");
      }

    | LPAREN expressao RPAREN
      {
          $$ = $2;
      }
    ;

%%

void yyerror(const char *message) {
    fprintf(stderr,
            "Erro sintatico na linha %d: %s\n",
            yylineno, message);
}
