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
                    "Erro semantico na linha %d: variavel '%s' utilizada antes de receber valor.\n",
                    node->line, node->text);

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

            case N_LIST:
                semantic_list(node->body);
                break;

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

        if (node->type == N_LIST) {
            emit_list(out, node->body);
            continue;
        }

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
    Node *statements = (root && root->type == N_LIST) ? root->body : root;

    errors = 0;

    semantic_list(statements);

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

    emit_list(out, statements);

    fclose(out);

    printf("Traducao concluida: %s\n", output_path);
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [--ast | --ast-only] arquivo.tojs [saida.js]\n",
            prog);
}

/* Funcao principal */

int main(int argc, char **argv) {
    int show_ast = 0;
    int ast_only = 0;
    const char *input_path = NULL;
    const char *output_path = "saida.js";
    int pos = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ast") == 0) {
            show_ast = 1;
        } else if (strcmp(argv[i], "--ast-only") == 0) {
            show_ast = 1;
            ast_only = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Opcao desconhecida: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else if (pos == 0) {
            input_path = argv[i];
            pos++;
        } else if (pos == 1) {
            output_path = argv[i];
            pos++;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!input_path) {
        print_usage(argv[0]);
        return 1;
    }

    yyin = fopen(input_path, "r");

    if (!yyin) {
        perror("Erro ao abrir arquivo de entrada");
        return 1;
    }

    int result = yyparse();

    fclose(yyin);

    if (result == 0) {
        if (show_ast) {
            ast_print(program_root, stdout);
        }

        if (!ast_only) {
            compile_program(program_root, output_path);
        }
    }

    ast_free(program_root);
    free_symbols();

    return (result != 0 || errors != 0) ? 1 : 0;
}
