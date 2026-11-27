#include "semantic.h"
#include <stdlib.h>
#include <string.h>

/* Estado de UM simbolo salvo num snapshot: se estava ativo e qual era seu tipo. */
typedef struct {
    Symbol *symbol;
    int active;
    ValueType type;
} SymbolState;

/*
 * "Foto" da tabela de simbolos em um instante (vetor de SymbolState).
 * E usada para analisar if/else e while: guarda-se o estado antes do bloco para
 * poder restaura-lo ou combina-lo com o estado depois do bloco.
 */
typedef struct {
    SymbolState *items;
    size_t count;
} SymbolSnapshot;

/* Copia de string no heap (strdup manual), com checagem de memoria. */
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

/* Predicados de tipo. is_numeric = int ou real. */
static int is_numeric(ValueType type) {
    return type == VAL_INT || type == VAL_REAL;
}

/*
 * Versoes permissivas: aceitam tambem VAL_DYNAMIC, porque um valor de tipo
 * dinamico PODE ser numerico/string/logico. Assim o analisador so acusa erro
 * quando tem CERTEZA de que o tipo e incompativel.
 */
static int can_be_numeric(ValueType type) {
    return is_numeric(type) || type == VAL_DYNAMIC;
}

static int can_be_string(ValueType type) {
    return type == VAL_STRING || type == VAL_DYNAMIC;
}

static int can_be_bool(ValueType type) {
    return type == VAL_BOOL || type == VAL_DYNAMIC;
}

/*
 * Une dois tipos quando dois caminhos de execucao se encontram (fim de if/else, laco):
 *   - algum erro            -> erro
 *   - tipos iguais          -> o mesmo tipo
 *   - int e real            -> real
 *   - qualquer outra mistura -> dinamico
 */
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

/* Nome do tipo em portugues para mensagens de erro e para --symbols. */
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

/* Inicia a tabela vazia e zera o contador de erros. */
void symtab_init(SymbolTable *table) {
    table->head = NULL;
    table->errors = 0;
}

/* Libera todos os simbolos (nome + struct) da lista ligada. */
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

/* Busca pelo nome entre TODOS os simbolos, ativos ou nao (busca linear na lista). */
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

/* Busca publica: so devolve o simbolo se ele estiver ATIVO neste ponto do fluxo (ou seja, ja recebeu valor). */
Symbol *symtab_find(const SymbolTable *table, const char *name) {
    Symbol *sym = symtab_find_any(table, name);
    if (sym && sym->active) {
        return sym;
    }
    return NULL;
}

/*
 * Registra uma atribuicao. Se a variavel ja existe, apenas a reativa e atualiza o tipo
 * (PtoJS permite mudar de tipo, como Python). Se e nova, cria o simbolo no FIM da lista,
 * guardando linha e profundidade da primeira atribuicao.
 */
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

/* Tira a foto da tabela: copia (ativo, tipo) de cada simbolo existente agora. */
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

/* Procura, dentro do snapshot, o estado salvo de um simbolo (NULL se ele nao existia na epoca). */
static const SymbolState *snapshot_find(const SymbolSnapshot *snap, const Symbol *sym) {
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->items[i].symbol == sym) {
            return &snap->items[i];
        }
    }
    return NULL;
}

/*
 * Volta a tabela ao estado do snapshot. Simbolos criados DEPOIS da foto ficam inativos
 * (ex.: variavel criada no then nao deve existir ainda ao analisar o else).
 */
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

/* Libera a memoria do snapshot. */
static void snapshot_free(SymbolSnapshot *snap) {
    free(snap->items);
    snap->items = NULL;
    snap->count = 0;
}

/* Verdadeiro se o no e um literal zero, mesmo com sinal unario: 0, 0.0, -0, +0.0.
 * Usado para detectar divisao/modulo por zero em tempo de analise. */
static int is_zero_literal(const Node *node) {
    while (node && node->type == N_UNARY &&
           (strcmp(node->text, "-") == 0 || strcmp(node->text, "+") == 0)) {
        node = node->left;
    }

    return node &&
           ((node->type == N_INT && node->int_value == 0) ||
            (node->type == N_REAL && node->number == 0.0));
}

/*
 * Verifica uma expressao e DEVOLVE o tipo dela (checagem bottom-up na arvore).
 * Em caso de erro: imprime a mensagem, soma em table->errors e devolve VAL_ERROR,
 * que se propaga para cima sem gerar novas mensagens.
 */
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

        /*
         * Uso de variavel: ela precisa estar ATIVA (ja ter recebido valor antes).
         * Senao: erro "utilizada antes de receber valor". Tambem marca `used`.
         */
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

        /* Unarios: `not` exige logico e resulta em logico; `+`/`-` exigem numerico e mantem o tipo. */
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

        /* Binarios: avalia os dois lados e aplica a regra do operador. Se algum lado ja deu erro, propaga o erro. */
        case N_BINARY: {
            ValueType left = check_expr(table, node->left);
            ValueType right = check_expr(table, node->right);
            const char *op = node->text;

            if (left == VAL_ERROR || right == VAL_ERROR) {
                return VAL_ERROR;
            }

            /*
             * `+`: int+int -> int; qualquer real -> real; string+string -> string (concatenacao).
             * Misturar string com numero e erro. Se envolve tipo dinamico, o resultado e dinamico.
             */
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

            /*
             * `-  *  /  %`: exigem numericos. Resultado: `/` sempre real (divisao verdadeira,
             * como no Python 3); os demais: real se algum lado for real, senao int.
             */
            if (strcmp(op, "-") == 0 || strcmp(op, "*") == 0 ||
                strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
                if (!can_be_numeric(left) || !can_be_numeric(right)) {
                    fprintf(stderr,
                            "Erro semantico na linha %d: operador '%s' exige operandos numericos, mas recebeu %s e %s.\n",
                            node->line, op, value_type_name(left), value_type_name(right));
                    table->errors++;
                    return VAL_ERROR;
                }
                /* Divisao/modulo por zero: detectada apenas quando o divisor e um LITERAL zero, com ou sem sinal (0, 0.0, -0). Checagem estatica. */
                if ((strcmp(op, "/") == 0 || strcmp(op, "%") == 0) && node->right &&
                    is_zero_literal(node->right)) {
                    fprintf(stderr,
                            "Erro semantico na linha %d: %s por zero.\n",
                            node->line, strcmp(op, "/") == 0 ? "divisao" : "modulo");
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

            /* Relacionais (< > <= >=): numero com numero ou string com string; resultado logico. */
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

            /*
             * Igualdade: tipos iguais (ou numericos entre si) sao compativeis; comparar com None e
             * sempre permitido; tipos diferentes (ex.: 1 == "a") sao erro. Resultado logico.
             */
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

            /* `and`/`or`: os dois operandos devem ser logicos (nao ha truthiness como no Python). Resultado logico. */
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

/* Valida a condicao de if/while: precisa resultar em valor logico (ou dinamico). */
static void check_condition(SymbolTable *table, const Node *cond, const char *stmt_name, int line) {
    ValueType cond_type = check_expr(table, cond);
    if (cond_type != VAL_ERROR && !can_be_bool(cond_type)) {
        fprintf(stderr,
                "Erro semantico na linha %d: condicao de '%s' deve resultar em valor logico, mas recebeu %s.\n",
                line, stmt_name, value_type_name(cond_type));
        table->errors++;
    }
}

/*
 * Percorre os comandos de um bloco (lista ligada via `next`). `depth` e a profundidade
 * de aninhamento (0 = topo do programa), usada para decidir depois quais variaveis
 * o codegen precisa declarar no topo do JS.
 */
static void check_block(SymbolTable *table, const Node *node, int depth) {
    while (node) {
        switch (node->type) {
            case N_LIST:
                check_block(table, node->body, depth);
                break;

            /*
             * Atribuicao: primeiro analisa o lado DIREITO (assim `x = x + 1` com x inexistente da erro)
             * e depois registra a variavel com o tipo inferido.
             */
            case N_ASSIGN: {
                ValueType rhs_type = check_expr(table, node->left);
                symtab_define(table, node->text, rhs_type, node->line, depth);
                break;
            }

            case N_PRINT:
                check_expr(table, node->left);
                break;

            /*
             * If: o PtoJS tem ESCOPO UNICO (como Python): variavel criada dentro de um bloco continua
             * existindo depois dele. Por isso a analise precisa combinar o estado dos dois ramos.
             */
            case N_IF: {
                check_condition(table, node->left, "if", node->line);

                /* Guarda o estado ANTES do if para poder analisar o else a partir do mesmo ponto de partida. */
                SymbolSnapshot pre = snapshot_take(table);
                check_block(table, node->body, depth + 1);
                SymbolSnapshot then_end = snapshot_take(table);

                if (node->else_body) {
                    /* O else comeca do mesmo estado em que o then comecou (variaveis do then ainda nao existem aqui). */
                    snapshot_restore(table, &pre);
                    check_block(table, node->else_body, depth + 1);

                    for (Symbol *curr = table->head; curr; curr = curr->next) {
                        /*
                         * Junta os dois ramos: variavel presente nos DOIS -> tipos unificados; so no then ou so no
                         * else -> continua valida com o tipo do ramo onde foi criada (fica ativa depois do if).
                         */
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
                        /*
                         * If SEM else: o bloco pode ou nao executar, entao para variaveis que ja existiam
                         * unifica-se o tipo anterior com o tipo apos o bloco. Variaveis novas do then continuam ativas.
                         */
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

            /*
             * While: o corpo pode mudar o tipo de variaveis usadas na condicao, entao a condicao e
             * checada ANTES do laco e de novo DEPOIS de analisar o corpo.
             */
            case N_WHILE: {
                /* Guarda o contador de erros para saber se a 1a checagem da condicao falhou. */
                int errors_before = table->errors;
                check_condition(table, node->left, "while", node->line);
                int cond_ok_initially = (table->errors == errors_before);

                SymbolSnapshot pre = snapshot_take(table);
                check_block(table, node->body, depth + 1);

                /* So reanalisa a condicao se ela estava correta na 1a vez (evita mensagem de erro duplicada). */
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

/*
 * Ponto de entrada da fase semantica. Inicia a tabela, analisa o programa a partir do topo
 * (profundidade 0) e devolve o total de erros.
 */
int semantic_analyze(Node *root, SymbolTable *table) {
    symtab_init(table);
    if (!root) {
        return 0;
    }

    const Node *statements = (root->type == N_LIST) ? root->body : root;
    check_block(table, statements, 0);
    return table->errors;
}

/* Imprime a tabela de simbolos em colunas (flag --symbols): nome, tipo, linha, nivel e se foi usada. */
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
