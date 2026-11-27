#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "semantic.h"
#include "codegen.h"

/*
 * Simbolos definidos pelo codigo gerado pelo Bison/Flex:
 * yyparse = ponto de entrada do analisador sintatico; yyin = arquivo lido pelo lexer.
 */
extern int yyparse(void);
extern FILE *yyin;

/* Mensagem de ajuda exibida em caso de erro de argumentos ou com -h/--help. */
static void print_usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [--ast | --ast-only] [--symbols] arquivo.tojs [saida.js]\n",
            prog);
}

/*
 * Orquestra as 4 fases do tradutor:
 *   1) lexica + sintatica (yyparse)  -> constroi a AST
 *   2) (opcional) imprime a AST      -> --ast / --ast-only
 *   3) analise semantica             -> tabela de simbolos + checagem de tipos
 *   4) geracao de codigo             -> escreve o .js
 * Se qualquer fase falhar, nenhum .js e gerado.
 */
int main(int argc, char **argv) {
    int show_ast = 0;
    int ast_only = 0;
    int show_symbols = 0;
    const char *input_path = NULL;
    const char *output_path = "saida.js";
    int pos = 0;

    /*
     * Leitura dos argumentos: as flags (--ast, --ast-only, --symbols) ligam opcoes;
     * o 1o argumento sem "-" e o arquivo de entrada e o 2o e o arquivo de saida
     * (padrao: saida.js).
     */
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

    /* Abre o .tojs e aponta o lexer para ele (yyin e o arquivo que o Flex le). */
    yyin = fopen(input_path, "r");
    if (!yyin) {
        perror("Erro ao abrir arquivo de entrada");
        return 1;
    }

    /* Fases lexica + sintatica. Retorna 0 se o programa e sintaticamente valido; erros lexicos encerram direto dentro do lexer. */
    int parse_result = yyparse();
    fclose(yyin);

    if (parse_result != 0) {
        ast_free(program_root);
        return 1;
    }

    if (show_ast) {
        ast_print(program_root, stdout);
    }

    /* Modo --ast-only: so analisa e imprime a AST, sem semantica nem geracao de codigo. */
    if (ast_only) {
        ast_free(program_root);
        return 0;
    }

    /* Fase semantica: preenche `symtab` e devolve o numero de erros encontrados (acumula todos antes de parar). */
    SymbolTable symtab;
    int sem_errors = semantic_analyze(program_root, &symtab);

    if (show_symbols) {
        semantic_print_symbols(&symtab, stdout);
    }

    /* So gera o JavaScript se NAO houve erro semantico. Depois libera tabela e AST. */
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
