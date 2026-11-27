#ifndef TOJS_CODEGEN_H
#define TOJS_CODEGEN_H

#include "ast.h"
#include "semantic.h"

/*
 * Gera o arquivo JavaScript em `output_path` a partir da AST validada e da
 * tabela de simbolos. Retorna 0 em sucesso e 1 em erro de I/O.
 */
int codegen_emit(const Node *root, const SymbolTable *symtab, const char *output_path);

#endif
