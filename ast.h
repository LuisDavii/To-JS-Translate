#ifndef TOJS_AST_H
#define TOJS_AST_H

typedef enum {
    N_LIST, N_ASSIGN, N_PRINT, N_IF, N_WHILE,
    N_INT, N_REAL, N_STRING, N_BOOL, N_NONE, N_ID,
    N_BINARY, N_UNARY
} NodeType;

typedef struct Node {
    NodeType type;
    char *text;
    double number;
    int boolean;
    struct Node *left, *right;
    struct Node *body, *else_body;
    struct Node *next;
} Node;

Node *node_new(NodeType type);
Node *node_text(NodeType type, const char *text);
Node *node_number(NodeType type, double value, const char *text);
Node *node_binary(const char *op, Node *left, Node *right);
Node *node_unary(const char *op, Node *operand);
Node *node_append(Node *list, Node *item);
void ast_free(Node *node);
void compile_program(Node *root, const char *output_path);
void yyerror(const char *message);
extern int yylineno;
extern Node *program_root;

#endif
