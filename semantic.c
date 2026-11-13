#include "semantic.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Symbol *symbol;
    int active;
    ValueType type;
} SymbolState;

typedef struct {
    SymbolState *items;
    size_t count;
} SymbolSnapshot;

static char *sem_duplicate(const char *text) {
    const char *src = text ? text : "";
    size_t len = strlen(src) + 1;
    char *copy = malloc(len);

    if (!copy) {
        fprintf(stderr, "Erro: memoria insuficiente na analise semantica.\n");
        exit(1);
    }

    memcpy(copy, src, len);
    return copy;
}

static int is_numeric(ValueType type) {
    return type == VAL_INT || type == VAL_REAL;
}

static int can_be_numeric(ValueType type) {
    return is_numeric(type) || type == VAL_DYNAMIC;
}

static int can_be_string(ValueType type) {
    return type == VAL_STRING || type == VAL_DYNAMIC;
}

static int can_be_bool(ValueType type) {
    return type == VAL_BOOL || type == VAL_DYNAMIC;
}

static ValueType unify_types(ValueType a, ValueType b) {
    if (a == VAL_ERROR || b == VAL_ERROR) {
        return VAL_ERROR;
    }
    if (a == b) {
        return a;
    }
    if ((a == VAL_INT && b == VAL_REAL) || (a == VAL_REAL && b == VAL_INT)) {
        return VAL_REAL;
    }
    return VAL_DYNAMIC;
}

const char *value_type_name(ValueType type) {
    switch (type) {
        case VAL_INT:     return "inteiro";
        case VAL_REAL:    return "real";
        case VAL_STRING:  return "string";
        case VAL_BOOL:    return "logico";
        case VAL_NONE:    return "None";
        case VAL_DYNAMIC: return "dinamico";
        case VAL_ERROR:   return "erro";
        default:          return "desconhecido";
    }
}

void symtab_init(SymbolTable *table) {
    table->head = NULL;
    table->errors = 0;
}

void symtab_free(SymbolTable *table) {
    Symbol *curr = table->head;
    while (curr) {
        Symbol *next = curr->next;
        free(curr->name);
        free(curr);
        curr = next;
    }
    table->head = NULL;
    table->errors = 0;
}

static Symbol *symtab_find_any(const SymbolTable *table, const char *name) {
    Symbol *curr = table->head;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

Symbol *symtab_find(const SymbolTable *table, const char *name) {
    Symbol *sym = symtab_find_any(table, name);
    if (sym && sym->active) {
        return sym;
    }
    return NULL;
}

static Symbol *symtab_define(SymbolTable *table, const char *name,
                             ValueType type, int line, int depth) {
    Symbol *existing = symtab_find_any(table, name);
    if (existing) {
        existing->active = 1;
        existing->type = type;
        return existing;
    }

    Symbol *sym = calloc(1, sizeof(Symbol));
    if (!sym) {
        fprintf(stderr, "Erro: memoria insuficiente na tabela de simbolos.\n");
        exit(1);
    }

    sym->name = sem_duplicate(name);
    sym->type = type;
    sym->decl_line = line;
    sym->decl_depth = depth;
    sym->active = 1;
    sym->used = 0;
    sym->next = NULL;

    if (!table->head) {
        table->head = sym;
    } else {
        Symbol *tail = table->head;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = sym;
    }

    return sym;
}

static SymbolSnapshot snapshot_take(const SymbolTable *table) {
    SymbolSnapshot snap = {NULL, 0};
    size_t count = 0;

    for (Symbol *curr = table->head; curr; curr = curr->next) {
        count++;
    }

    if (count == 0) {
        return snap;
    }

    snap.items = calloc(count, sizeof(SymbolState));
    if (!snap.items) {
        fprintf(stderr, "Erro: memoria insuficiente ao salvar estado da tabela.\n");
        exit(1);
    }

    size_t i = 0;
    for (Symbol *curr = table->head; curr; curr = curr->next) {
        snap.items[i].symbol = curr;
        snap.items[i].active = curr->active;
        snap.items[i].type = curr->type;
        i++;
    }
    snap.count = count;
    return snap;
}

static const SymbolState *snapshot_find(const SymbolSnapshot *snap, const Symbol *sym) {
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->items[i].symbol == sym) {
            return &snap->items[i];
        }
    }
    return NULL;
}

static void snapshot_restore(SymbolTable *table, const SymbolSnapshot *snap) {
    for (Symbol *curr = table->head; curr; curr = curr->next) {
        const SymbolState *saved = snapshot_find(snap, curr);
        if (saved) {
            curr->active = saved->active;
            curr->type = saved->type;
        } else {
            curr->active = 0;
        }
    }
}

static void snapshot_free(SymbolSnapshot *snap) {
    free(snap->items);
    snap->items = NULL;
    snap->count = 0;
}

static ValueType check_expr(SymbolTable *table, const Node *node) {
    if (!node) {
        return VAL_UNKNOWN;
    }

    switch (node->type) {
        case N_INT:
            return VAL_INT;

        case N_REAL:
            return VAL_REAL;

        case N_STRING:
            return VAL_STRING;

        case N_BOOL:
            return VAL_BOOL;

        case N_NONE:
            return VAL_NONE;

        case N_ID: {
            Symbol *sym = symtab_find(table, node->text);
            if (!sym) {
                fprintf(stderr,
                        "Erro semantico na linha %d: variavel '%s' utilizada antes de receber valor.\n",
                        node->line, node->text);
                table->errors++;
                return VAL_ERROR;
            }
            sym->used = 1;
            return sym->type;
        }

        case N_UNARY: {
            ValueType op_type = check_expr(table, node->left);
            if (op_type == VAL_ERROR) {
                return VAL_ERROR;
            }

            if (strcmp(node->text, "not") == 0) {
                if (!can_be_bool(op_type)) {
                    fprintf(stderr,
                            "Erro semantico na linha %d: operador logico 'not' exige operando logico, mas recebeu %s.\n",
                            node->line, value_type_name(op_type));
                    table->errors++;
                    return VAL_ERROR;
                }
                return VAL_BOOL;
            }

            if (!can_be_numeric(op_type)) {
                fprintf(stderr,
                        "Erro semantico na linha %d: operador unario '%s' exige operando numerico, mas recebeu %s.\n",
                        node->line, node->text, value_type_name(op_type));
                table->errors++;
                return VAL_ERROR;
            }
            return op_type;
        }

        case N_BINARY: {
            ValueType left = check_expr(table, node->left);
            ValueType right = check_expr(table, node->right);
            const char *op = node->text;

            if (left == VAL_ERROR || right == VAL_ERROR) {
                return VAL_ERROR;
            }

            if (strcmp(op, "+") == 0) {
                if (is_numeric(left) && is_numeric(right)) {
                    return (left == VAL_REAL || right == VAL_REAL) ? VAL_REAL : VAL_INT;
                }
                if (left == VAL_STRING && right == VAL_STRING) {
                    return VAL_STRING;
                }
                if ((can_be_numeric(left) && can_be_numeric(right)) ||
                    (can_be_string(left) && can_be_string(right))) {
                    return VAL_DYNAMIC;
                }

                fprintf(stderr,
                        "Erro semantico na linha %d: operador '+' exige dois operandos numericos ou duas strings, mas recebeu %s e %s.\n",
                        node->line, value_type_name(left), value_type_name(right));
                table->errors++;
                return VAL_ERROR;
            }

            if (strcmp(op, "-") == 0 || strcmp(op, "*") == 0 ||
                strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
                if (!can_be_numeric(left) || !can_be_numeric(right)) {
                    fprintf(stderr,
                            "Erro semantico na linha %d: operador '%s' exige operandos numericos, mas recebeu %s e %s.\n",
                            node->line, op, value_type_name(left), value_type_name(right));
                    table->errors++;
                    return VAL_ERROR;
                }
                if (strcmp(op, "/") == 0) {
                    return VAL_REAL;
                }
                if (left == VAL_DYNAMIC || right == VAL_DYNAMIC) {
                    return VAL_DYNAMIC;
                }
                return (left == VAL_REAL || right == VAL_REAL) ? VAL_REAL : VAL_INT;
            }

            if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                if ((can_be_numeric(left) && can_be_numeric(right)) ||
                    (can_be_string(left) && can_be_string(right))) {
                    return VAL_BOOL;
                }

                fprintf(stderr,
                        "Erro semantico na linha %d: operador relacional '%s' aplicado a tipos incompativeis (%s e %s).\n",
                        node->line, op, value_type_name(left), value_type_name(right));
                table->errors++;
                return VAL_ERROR;
            }

            if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
                int compatible =
                    (is_numeric(left) && is_numeric(right)) ||
                    (left == VAL_STRING && right == VAL_STRING) ||
                    (left == VAL_BOOL && right == VAL_BOOL) ||
                    (left == VAL_NONE || right == VAL_NONE) ||
                    (left == VAL_DYNAMIC || right == VAL_DYNAMIC);

                if (!compatible) {
                    fprintf(stderr,
                            "Erro semantico na linha %d: operador de igualdade '%s' aplicado a tipos incompativeis (%s e %s).\n",
                            node->line, op, value_type_name(left), value_type_name(right));
                    table->errors++;
                    return VAL_ERROR;
                }
                return VAL_BOOL;
            }

            if (strcmp(op, "and") == 0 || strcmp(op, "or") == 0) {
                if (!can_be_bool(left) || !can_be_bool(right)) {
                    fprintf(stderr,
                            "Erro semantico na linha %d: operador logico '%s' exige operandos logicos, mas recebeu %s e %s.\n",
                            node->line, op, value_type_name(left), value_type_name(right));
                    table->errors++;
                    return VAL_ERROR;
                }
                return VAL_BOOL;
            }

            return VAL_UNKNOWN;
        }

        default:
            return VAL_UNKNOWN;
    }
}

static void check_condition(SymbolTable *table, const Node *cond, const char *stmt_name, int line) {
    ValueType cond_type = check_expr(table, cond);
    if (cond_type != VAL_ERROR && !can_be_bool(cond_type)) {
        fprintf(stderr,
                "Erro semantico na linha %d: condicao de '%s' deve resultar em valor logico, mas recebeu %s.\n",
                line, stmt_name, value_type_name(cond_type));
        table->errors++;
    }
}

static void check_block(SymbolTable *table, const Node *node, int depth) {
    while (node) {
        switch (node->type) {
            case N_LIST:
                check_block(table, node->body, depth);
                break;

            case N_ASSIGN: {
                ValueType rhs_type = check_expr(table, node->left);
                symtab_define(table, node->text, rhs_type, node->line, depth);
                break;
            }

            case N_PRINT:
                check_expr(table, node->left);
                break;

            case N_IF: {
                check_condition(table, node->left, "if", node->line);

                SymbolSnapshot pre = snapshot_take(table);
                check_block(table, node->body, depth + 1);
                SymbolSnapshot then_end = snapshot_take(table);

                if (node->else_body) {
                    snapshot_restore(table, &pre);
                    check_block(table, node->else_body, depth + 1);

                    for (Symbol *curr = table->head; curr; curr = curr->next) {
                        const SymbolState *t = snapshot_find(&then_end, curr);
                        int in_then = (t && t->active);
                        int in_else = curr->active;

                        if (in_then && in_else) {
                            curr->active = 1;
                            curr->type = unify_types(t->type, curr->type);
                        } else if (in_then) {
                            curr->active = 1;
                            curr->type = t->type;
                        } else if (in_else) {
                            curr->active = 1;
                        }
                    }
                } else {
                    for (Symbol *curr = table->head; curr; curr = curr->next) {
                        const SymbolState *p = snapshot_find(&pre, curr);
                        if (p && p->active) {
                            curr->type = unify_types(p->type, curr->type);
                        }
                    }
                }

                snapshot_free(&then_end);
                snapshot_free(&pre);
                break;
            }

            case N_WHILE: {
                int errors_before = table->errors;
                check_condition(table, node->left, "while", node->line);
                int cond_ok_initially = (table->errors == errors_before);

                SymbolSnapshot pre = snapshot_take(table);
                check_block(table, node->body, depth + 1);

                if (cond_ok_initially) {
                    check_condition(table, node->left, "while", node->line);
                }

                for (Symbol *curr = table->head; curr; curr = curr->next) {
                    const SymbolState *p = snapshot_find(&pre, curr);
                    if (p && p->active) {
                        curr->type = unify_types(p->type, curr->type);
                    }
                }

                snapshot_free(&pre);
                break;
            }

            default:
                break;
        }

        node = node->next;
    }
}

int semantic_analyze(Node *root, SymbolTable *table) {
    symtab_init(table);
    if (!root) {
        return 0;
    }

    const Node *statements = (root->type == N_LIST) ? root->body : root;
    check_block(table, statements, 0);
    return table->errors;
}

void semantic_print_symbols(const SymbolTable *table, FILE *out) {
    if (!out) {
        out = stdout;
    }

    fprintf(out, "=== Tabela de Simbolos ===\n");
    if (!table || !table->head) {
        fprintf(out, "(nenhuma variavel declarada)\n");
        return;
    }

    for (const Symbol *curr = table->head; curr; curr = curr->next) {
        fprintf(out, "  %-16s | tipo: %-10s | linha: %-3d | nivel: %d | usada: %s\n",
                curr->name,
                value_type_name(curr->type),
                curr->decl_line,
                curr->decl_depth,
                curr->used ? "sim" : "nao");
    }
}
