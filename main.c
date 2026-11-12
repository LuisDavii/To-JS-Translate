
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yyparse(void);
extern FILE *yyin;
extern int yylineno;

static int errors = 0;
static int output_indent = 0;

/* Tabela de simbolos */

typedef struct Symbol {
    char *name;
    struct Symbol *next;
} Symbol;

static Symbol *symbols = NULL;

/* Copia uma string */

static char *duplicate(const char *text) {
    char *copy = malloc(strlen(text) + 1);

    if (!copy) {
        fprintf(stderr, "Erro: memoria insuficiente\n");
        exit(1);
    }

    strcpy(copy, text);
    return copy;
}

/* Criacao dos nos da AST */

Node *node_new(NodeType type) {
    Node *node = calloc(1, sizeof(Node));

    if (!node) {
        fprintf(stderr, "Erro: memoria insuficiente\n");
        exit(1);
    }

    node->type = type;

    return node;
}

Node *node_text(NodeType type, const char *text) {
    Node *node = node_new(type);

    node->text = duplicate(text ? text : "");

    return node;
}

Node *node_binary(const char *op, Node *left, Node *right) {
    Node *node = node_text(N_BINARY, op);

    node->left = left;
    node->right = right;

    return node;
}

Node *node_unary(const char *op, Node *operand) {
    Node *node = node_text(N_UNARY, op);

    node->left = operand;

    return node;
}

/* Adiciona um comando a lista da AST */

Node *node_append(Node *list, Node *item) {
    if (!item)
        return list;

    if (!list)
        return item;

    Node *current = list;

    while (current->next)
        current = current->next;

    current->next = item;

    return list;
}

/* Libera a memoria da AST */

void ast_free(Node *node) {
    while (node) {
        Node *next = node->next;

        ast_free(node->left);
        ast_free(node->right);
        ast_free(node->body);
        ast_free(node->else_body);

        free(node->text);
        free(node);

        node = next;
    }
}

/* Busca uma variavel na tabela de simbolos */

static Symbol *find_symbol(const char *name) {
    Symbol *current = symbols;

    while (current) {
        if (strcmp(current->name, name) == 0)
            return current;

        current = current->next;
    }

    return NULL;
}

/* Registra uma variavel */

static void declare_symbol(const char *name) {
    if (find_symbol(name))
        return;

    Symbol *symbol = calloc(1, sizeof(Symbol));

    if (!symbol) {
        fprintf(stderr, "Erro: memoria insuficiente\n");
        exit(1);
    }

    symbol->name = duplicate(name);
    symbol->next = symbols;

    symbols = symbol;
}

/* Libera a tabela de simbolos */

static void free_symbols(void) {
    while (symbols) {
        Symbol *next = symbols->next;

        free(symbols->name);
        free(symbols);

        symbols = next;
    }
}

/* Analise semantica das expressoes */

static void semantic_expr(Node *node) {
    if (!node)
        return;

    if (node->type == N_ID) {
        if (!find_symbol(node->text)) {
            fprintf(stderr,
                    "Erro semantico: variavel '%s' utilizada antes de receber valor.\n",
                    node->text);

            errors++;
        }
    }

    semantic_expr(node->left);
    semantic_expr(node->right);
}

/* Analise semantica dos comandos */

static void semantic_list(Node *node) {
    while (node) {

        switch (node->type) {

            case N_ASSIGN:
                semantic_expr(node->left);
                declare_symbol(node->text);
                break;

            case N_PRINT:
                semantic_expr(node->left);
                break;

            case N_IF:
                semantic_expr(node->left);
                semantic_list(node->body);
                semantic_list(node->else_body);
                break;

            case N_WHILE:
                semantic_expr(node->left);
                semantic_list(node->body);
                break;

            default:
                break;
        }

        node = node->next;
    }
}

/* Controle da indentacao do JavaScript */

static void indent(FILE *out) {
    for (int i = 0; i < output_indent; i++)
        fputs("    ", out);
}

/* Geracao das expressoes JavaScript */

static void emit_expr(FILE *out, Node *node) {
    if (!node)
        return;

    switch (node->type) {

        case N_INT:
        case N_REAL:
        case N_STRING:
        case N_BOOL:
        case N_NONE:
        case N_ID:
            fputs(node->text, out);
            break;

        case N_BINARY:
            fputc('(', out);

            emit_expr(out, node->left);

            if (strcmp(node->text, "and") == 0)
                fputs(" && ", out);

            else if (strcmp(node->text, "or") == 0)
                fputs(" || ", out);

            else
                fprintf(out, " %s ", node->text);

            emit_expr(out, node->right);

            fputc(')', out);
            break;

        case N_UNARY:
            if (strcmp(node->text, "not") == 0)
                fputc('!', out);
            else
                fputs(node->text, out);

            emit_expr(out, node->left);
            break;

        default:
            break;
    }
}

/* Geracao dos comandos JavaScript */

static void emit_list(FILE *out, Node *node) {

    for (; node; node = node->next) {

        indent(out);

        switch (node->type) {

            case N_ASSIGN: {
                static char *emitted[1024];
                static int count = 0;

                int seen = 0;

                for (int i = 0; i < count; i++) {
                    if (strcmp(emitted[i], node->text) == 0) {
                        seen = 1;
                        break;
                    }
                }

                if (!seen) {
                    if (count >= 1024) {
                        fprintf(stderr, "Limite de variaveis excedido.\n");
                        exit(1);
                    }

                    emitted[count++] = node->text;

                    fprintf(out, "let %s = ", node->text);
                }
                else {
                    fprintf(out, "%s = ", node->text);
                }

                emit_expr(out, node->left);

                fputs(";\n", out);
                break;
            }

            case N_PRINT:
                fputs("console.log(", out);

                emit_expr(out, node->left);

                fputs(");\n", out);
                break;

            case N_IF:
                fputs("if (", out);

                emit_expr(out, node->left);

                fputs(") {\n", out);

                output_indent++;
                emit_list(out, node->body);
                output_indent--;

                indent(out);
                fputc('}', out);

                if (node->else_body) {
                    fputs(" else {\n", out);

                    output_indent++;
                    emit_list(out, node->else_body);
                    output_indent--;

                    indent(out);
                    fputs("}\n", out);
                }
                else {
                    fputc('\n', out);
                }

                break;

            case N_WHILE:
                fputs("while (", out);

                emit_expr(out, node->left);

                fputs(") {\n", out);

                output_indent++;
                emit_list(out, node->body);
                output_indent--;

                indent(out);
                fputs("}\n", out);

                break;

            default:
                break;
        }
    }
}

/* Compilacao do programa */

void compile_program(Node *root, const char *output_path) {

    errors = 0;

    semantic_list(root);

    if (errors > 0) {
        fprintf(stderr,
                "Traducao cancelada: %d erro(s) semantico(s).\n",
                errors);

        return;
    }

    FILE *out = fopen(output_path, "w");

    if (!out) {
        perror("Erro ao criar arquivo JavaScript");
        return;
    }

    emit_list(out, root);

    fclose(out);

    printf("Traducao concluida: %s\n", output_path);
}

/* Funcao principal */

int main(int argc, char **argv) {

    if (argc < 2 || argc > 3) {
        fprintf(stderr,
                "Uso: %s arquivo.tojs [saida.js]\n",
                argv[0]);

        return 1;
    }

    yyin = fopen(argv[1], "r");

    if (!yyin) {
        perror("Erro ao abrir arquivo de entrada");
        return 1;
    }

    int result = yyparse();

    fclose(yyin);

    if (result == 0) {
        compile_program(
            program_root,
            argc == 3 ? argv[2] : "saida.js"
        );
    }

    ast_free(program_root);
    free_symbols();

    return (result != 0 || errors != 0) ? 1 : 0;
}
