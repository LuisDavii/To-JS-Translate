#ifndef TOJS_CODEGEN_H
#define TOJS_CODEGEN_H

#include "ast.h"
#include "semantic.h"

int codegen_emit(const Node *root, const SymbolTable *symtab, const char *output_path);

#endif
