#ifndef TOJS_SEMANTIC_H
#define TOJS_SEMANTIC_H

#include <stdio.h>
#include "ast.h"

/*
 * Tipos de valor que a analise semantica conhece.
 * O PtoJS nao tem declaracao de tipos (como Python): o tipo de cada variavel
 * e INFERIDO a partir do valor atribuido.
 *
 *   VAL_UNKNOWN -> nao determinado / no sem tipo (ex.: ausencia de expressao)
 *   VAL_INT, VAL_REAL, VAL_STRING, VAL_BOOL, VAL_NONE -> tipos primitivos
 *   VAL_DYNAMIC -> o tipo depende do caminho de execucao (ex.: x e int num ramo
 *                  do if e string no outro). Aceito em qualquer operacao para
 *                  evitar falsos erros; o JS decide em tempo de execucao.
 *   VAL_ERROR   -> expressao ja tem erro; serve para NAO repetir mensagens em
 *                  cascata sobre a mesma causa.
 */
typedef enum {
    VAL_UNKNOWN = 0,
    VAL_INT,
    VAL_REAL,
    VAL_STRING,
    VAL_BOOL,
    VAL_NONE,
    VAL_DYNAMIC,
    VAL_ERROR
} ValueType;

/*
 * Entrada da tabela de simbolos (uma por variavel), numa lista ligada.
 *
 *   name       -> nome da variavel
 *   type       -> tipo inferido atual
 *   decl_line  -> linha da primeira atribuicao
 *   decl_depth -> profundidade de bloco da primeira atribuicao (0 = topo).
 *                 O codegen usa isso: se > 0, declara `let x;` no topo do JS.
 *   active     -> 1 se a variavel ja "existe" neste ponto do fluxo de execucao
 *   used       -> 1 se alguma expressao ja leu a variavel (para --symbols)
 */
typedef struct Symbol {
    char *name;
    ValueType type;
    int decl_line;
    int decl_depth;
    int active;
    int used;
    struct Symbol *next;
} Symbol;

/* Tabela de simbolos: lista ligada de Symbol + contador de erros semanticos acumulados. */
typedef struct {
    Symbol *head;
    int errors;
} SymbolTable;

void symtab_init(SymbolTable *table);
void symtab_free(SymbolTable *table);
Symbol *symtab_find(const SymbolTable *table, const char *name);
const char *value_type_name(ValueType type);

/*
 * Percorre a AST, infere/verifica tipos e preenche a tabela.
 * Retorna a quantidade de erros semanticos (0 = tudo certo).
 */
int semantic_analyze(Node *root, SymbolTable *table);
void semantic_print_symbols(const SymbolTable *table, FILE *out);

#endif
