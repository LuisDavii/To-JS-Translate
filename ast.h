#ifndef TOJS_AST_H
#define TOJS_AST_H

#include <stdio.h>

/*
 * Tipos de no da AST (Arvore Sintatica Abstrata).
 * Cada construcao da linguagem PtoJS vira um destes nos.
 * A AST e o "meio-termo" entre o codigo-fonte (.tojs) e o JavaScript gerado:
 * o parser a constroi, o semantic.c a valida e o codegen.c a percorre.
 */
typedef enum {
    N_LIST,
    N_ASSIGN,
    N_PRINT,
    N_IF,
    N_WHILE,
    N_INT,
    N_REAL,
    N_STRING,
    N_BOOL,
    N_NONE,
    N_ID,
    N_BINARY,
    N_UNARY
} NodeType;

/*
 * Um unico struct serve para TODOS os tipos de no (estilo "no generico").
 * Qual campo e usado depende de `type`:
 *
 *   N_LIST   -> body = primeiro comando do programa
 *   N_ASSIGN -> text = nome da variavel, left = expressao atribuida
 *   N_PRINT  -> left = expressao impressa
 *   N_IF     -> left = condicao, body = bloco "then", else_body = bloco "else" (ou NULL)
 *   N_WHILE  -> left = condicao, body = bloco do laco
 *   N_INT    -> text = literal original ("007"), int_value = valor numerico
 *   N_REAL   -> text = literal original ("1.50"), number = valor numerico
 *   N_STRING -> text = literal COM as aspas, como foi escrito no codigo
 *   N_BOOL   -> boolean = 0/1, text = "true"/"false" (ja no formato JS)
 *   N_NONE   -> text = "null" (ja no formato JS)
 *   N_ID     -> text = nome da variavel
 *   N_BINARY -> text = operador ("+", "and", "=="...), left e right = operandos
 *   N_UNARY  -> text = operador ("-", "+", "not"), left = operando
 *
 * `next` encadeia os comandos de um mesmo bloco numa lista ligada:
 *   comando1 -> comando2 -> comando3 -> NULL
 * Ja `left`/`right`/`body`/`else_body` apontam para sub-arvores (filhos).
 */
typedef struct Node {
    NodeType type;
    int line;
    char *text;
    long long int_value;
    double number;
    int boolean;
    struct Node *left;
    struct Node *right;
    struct Node *body;
    struct Node *else_body;
    struct Node *next;
} Node;

/*
 * Construtores de nos. Todos recebem a linha do codigo-fonte (`line`)
 * para que os erros semanticos possam informar onde o problema ocorreu.
 */
Node *node_new(NodeType type, int line);
Node *node_list(Node *items, int line);
Node *node_text(NodeType type, const char *text, int line);
Node *node_number(NodeType type, double value, const char *text, int line);
Node *node_bool(int value, int line);
Node *node_binary(const char *op, Node *left, Node *right, int line);
Node *node_unary(const char *op, Node *operand, int line);
/* Acrescenta `item` ao FIM da lista ligada `list` (usado para montar blocos de comandos). */
Node *node_append(Node *list, Node *item);

/* Utilitarios de depuracao: nome do tipo, contagem de nos, impressao da arvore (--ast) e liberacao de memoria. */
const char *node_type_name(NodeType type);
int ast_count_nodes(const Node *node);
void ast_print(const Node *node, FILE *out);
void ast_free(Node *node);

/*
 * yyerror: funcao de erro chamada pelo Bison (definida no final do parser.y).
 * program_root: raiz da AST, preenchida pela regra `programa` do parser.
 */
void yyerror(const char *message);

extern int yylineno;
extern Node *program_root;

#endif
