#ifndef TOJS_AST_H
#define TOJS_AST_H

#include <stdio.h>

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

Node *node_new(NodeType type, int line);
Node *node_list(Node *items, int line);
Node *node_text(NodeType type, const char *text, int line);
Node *node_number(NodeType type, double value, const char *text, int line);
Node *node_bool(int value, int line);
Node *node_binary(const char *op, Node *left, Node *right, int line);
Node *node_unary(const char *op, Node *operand, int line);
Node *node_append(Node *list, Node *item);

const char *node_type_name(NodeType type);
int ast_count_nodes(const Node *node);
void ast_print(const Node *node, FILE *out);
void ast_free(Node *node);

void compile_program(Node *root, const char *output_path);
void yyerror(const char *message);

extern int yylineno;
extern Node *program_root;

#endif
