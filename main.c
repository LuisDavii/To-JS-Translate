#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "semantic.h"
#include "codegen.h"

extern int yyparse(void);
extern FILE *yyin;

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [--ast | --ast-only] [--symbols] arquivo.tojs [saida.js]\n",
            prog);
}

int main(int argc, char **argv) {
    int show_ast = 0;
    int ast_only = 0;
    int show_symbols = 0;
    const char *input_path = NULL;
    const char *output_path = "saida.js";
    int pos = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ast") == 0) {
            show_ast = 1;
        } else if (strcmp(argv[i], "--ast-only") == 0) {
            show_ast = 1;
            ast_only = 1;
        } else if (strcmp(argv[i], "--symbols") == 0) {
            show_symbols = 1;
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

    int parse_result = yyparse();
    fclose(yyin);

    if (parse_result != 0) {
        ast_free(program_root);
        return 1;
    }

    if (show_ast) {
        ast_print(program_root, stdout);
    }

    if (ast_only) {
        ast_free(program_root);
        return 0;
    }

    SymbolTable symtab;
    int sem_errors = semantic_analyze(program_root, &symtab);

    if (show_symbols) {
        semantic_print_symbols(&symtab, stdout);
    }

    int exit_code = 0;

    if (sem_errors > 0) {
        fprintf(stderr,
                "Traducao cancelada: %d erro(s) semantico(s).\n",
                sem_errors);
        exit_code = 1;
    } else if (codegen_emit(program_root, &symtab, output_path) != 0) {
        exit_code = 1;
    } else {
        printf("Traducao concluida: %s\n", output_path);
    }

    symtab_free(&symtab);
    ast_free(program_root);

    return exit_code;
}
