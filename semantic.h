#ifndef TOJS_SEMANTIC_H
#define TOJS_SEMANTIC_H

#include <stdio.h>
#include "ast.h"

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

typedef struct Symbol {
    char *name;
    ValueType type;
    int decl_line;
    int decl_depth;
    int active;
    int used;
    struct Symbol *next;
} Symbol;

typedef struct {
    Symbol *head;
    int errors;
} SymbolTable;

void symtab_init(SymbolTable *table);
void symtab_free(SymbolTable *table);
Symbol *symtab_find(const SymbolTable *table, const char *name);
const char *value_type_name(ValueType type);

int semantic_analyze(Node *root, SymbolTable *table);
void semantic_print_symbols(const SymbolTable *table, FILE *out);

#endif
