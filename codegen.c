#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Estado do gerador: arquivo de saida, nivel de indentacao atual e a lista de variaveis
 * JA declaradas com `let` (para declarar so na primeira atribuicao).
 */
typedef struct {
    FILE *out;
    int indent_level;
    const char **declared;
    size_t declared_count;
    size_t declared_cap;
} CodeGen;

/*
 * Verdadeiro se o nome e palavra reservada do JavaScript (ou `console`).
 * Ex.: uma variavel PtoJS chamada `new` ou `var` quebraria o JS gerado.
 */
static int is_js_reserved(const char *name) {
    static const char *const keywords[] = {
        "await", "break", "case", "catch", "class", "console", "const", "continue",
        "debugger", "default", "delete", "do", "else", "enum", "export",
        "extends", "false", "finally", "for", "function", "if", "implements",
        "import", "in", "instanceof", "interface", "let", "new", "null",
        "package", "private", "protected", "public", "return", "static",
        "super", "switch", "this", "throw", "true", "try", "typeof", "var",
        "void", "while", "with", "yield", NULL
    };

    for (size_t i = 0; keywords[i] != NULL; i++) {
        if (strcmp(name, keywords[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Escreve um identificador; se colidir com o JS, ganha o prefixo `_tojs_` (ex.: var -> _tojs_var). */
static void emit_ident(FILE *out, const char *name) {
    if (is_js_reserved(name)) {
        fprintf(out, "_tojs_%s", name);
    } else {
        fputs(name, out);
    }
}

/* Ja houve `let` para este nome? (busca linear na lista) */
static int is_declared(const CodeGen *cg, const char *name) {
    for (size_t i = 0; i < cg->declared_count; i++) {
        if (strcmp(cg->declared[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Registra o nome como declarado. O vetor cresce dobrando a capacidade (16, 32, 64...). */
static void mark_declared(CodeGen *cg, const char *name) {
    if (is_declared(cg, name)) {
        return;
    }

    if (cg->declared_count == cg->declared_cap) {
        size_t new_cap = cg->declared_cap == 0 ? 16 : cg->declared_cap * 2;
        const char **resized = realloc(cg->declared, new_cap * sizeof(const char *));
        if (!resized) {
            fprintf(stderr, "Erro: memoria insuficiente na geracao de codigo.\n");
            exit(1);
        }
        cg->declared = resized;
        cg->declared_cap = new_cap;
    }

    cg->declared[cg->declared_count++] = name;
}

/* Escreve 4 espacos por nivel de indentacao atual. */
static void emit_indent(const CodeGen *cg) {
    for (int i = 0; i < cg->indent_level; i++) {
        fputs("    ", cg->out);
    }
}

/*
 * Nivel de precedencia de um no (maior = liga mais forte):
 *   1 or | 2 and | 3 comparacoes | 4 + - | 5 * / % | 6 unarios | 7 atomos (literais, ids)
 */
static int expr_prec(const Node *node) {
    if (!node) {
        return 7;
    }

    if (node->type == N_UNARY) {
        return 6;
    }

    if (node->type == N_BINARY) {
        const char *op = node->text;
        if (strcmp(op, "or") == 0) {
            return 1;
        }
        if (strcmp(op, "and") == 0) {
            return 2;
        }
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
            strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
            strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
            return 3;
        }
        if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) {
            return 4;
        }
        if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
            return 5;
        }
    }

    return 7;
}

/*
 * Traducao de operadores PtoJS -> JS: and -> &&, or -> ||, == -> ===, != -> !==.
 * (=== e !== evitam a coercao implicita de tipos do JavaScript.) Os demais passam iguais.
 */
static const char *map_binary_op(const char *op) {
    if (strcmp(op, "and") == 0) {
        return "&&";
    }
    if (strcmp(op, "or") == 0) {
        return "||";
    }
    if (strcmp(op, "==") == 0) {
        return "===";
    }
    if (strcmp(op, "!=") == 0) {
        return "!==";
    }
    return op;
}

/*
 * Escreve uma expressao em JS. IMPORTANTE: a AST NAO guarda parenteses (o parser descarta
 * `( expr )`), entao eles sao RECONSTRUIDOS aqui comparando a precedencia do no
 * (`p`) com a do pai (`parent_prec`), so quando necessario.
 */
static void emit_expr(CodeGen *cg, const Node *node, int parent_prec,
                      int is_right_child, const char *parent_op) {
    if (!node) {
        return;
    }

    int p = expr_prec(node);
    int need_parens = 0;

    /*
     * Regras para colocar parenteses:
     *   - filho com precedencia MENOR que a do pai -> sempre (ex.: (a + b) * c)
     *   - mesma precedencia:
     *       * unario dentro de unario -> evita gerar `--x` (que em JS seria decremento)
     *       * comparacao dentro de comparacao -> sempre
     *       * filho da DIREITA de - / % -> preserva a associatividade (a - (b - c))
     *   - unario +/- no lado direito de um + ou - binario -> a - (-b)
     */
    if (parent_prec > 0) {
        if (p < parent_prec) {
            need_parens = 1;
        } else if (p == parent_prec) {
            if (node->type == N_UNARY) {
                need_parens = 1;
            } else if (p == 3) {
                need_parens = 1;
            } else if (is_right_child && parent_op &&
                       (strcmp(parent_op, "-") == 0 ||
                        strcmp(parent_op, "/") == 0 ||
                        strcmp(parent_op, "%") == 0)) {
                need_parens = 1;
            }
        } else if (node->type == N_UNARY && parent_op &&
                   (strcmp(parent_op, "+") == 0 || strcmp(parent_op, "-") == 0) &&
                   (strcmp(node->text, "+") == 0 || strcmp(node->text, "-") == 0) &&
                   is_right_child) {
            need_parens = 1;
        }
    }

    if (need_parens) {
        fputc('(', cg->out);
    }

    switch (node->type) {
        /* Inteiro: remove zeros a esquerda (007 -> 7), pois em JS isso seria octal/invalido. */
        case N_INT: {
            const char *s = node->text ? node->text : "0";
            while (s[0] == '0' && s[1] != '\0') {
                s++;
            }
            fputs(s, cg->out);
            break;
        }

        /* Real: tambem tira zeros a esquerda, mas preserva o `0` antes do ponto (007.5 -> 7.5; 0.5 continua). */
        case N_REAL: {
            const char *s = node->text ? node->text : "0.0";
            while (s[0] == '0' && s[1] != '.' && s[1] != '\0') {
                s++;
            }
            fputs(s, cg->out);
            break;
        }

        /* Strings, booleanos e None ja vem prontos da AST no formato JS (aspas originais, true/false, null). */
        case N_STRING:
        case N_BOOL:
        case N_NONE:
            fputs(node->text, cg->out);
            break;

        case N_ID:
            emit_ident(cg->out, node->text);
            break;

        /* Binario: esquerda, operador traduzido com espacos, direita. Os filhos recebem a precedencia deste no. */
        case N_BINARY:
            /* Modulo: o resto do JS tem o sinal do DIVIDENDO (-7 % 3 = -1), mas o do
             * Python tem o sinal do DIVISOR (-7 % 3 = 2). Para manter o comportamento
             * do Python, `a % b` vira `((a % b) + b) % b`. (As expressoes da linguagem
             * nao tem efeito colateral, entao repetir `b` e seguro.) */
            if (strcmp(node->text, "%") == 0) {
                fputs("((", cg->out);
                emit_expr(cg, node->left, p, 0, node->text);
                fputs(" % ", cg->out);
                emit_expr(cg, node->right, p, 1, node->text);
                fputs(") + ", cg->out);
                emit_expr(cg, node->right, p, 1, node->text);
                fputs(") % ", cg->out);
                emit_expr(cg, node->right, p, 1, node->text);
                break;
            }

            emit_expr(cg, node->left, p, 0, node->text);
            fprintf(cg->out, " %s ", map_binary_op(node->text));
            emit_expr(cg, node->right, p, 1, node->text);
            break;

        /* Unario: `not` vira `!`; `+`/`-` sao copiados; depois vem o operando. */
        case N_UNARY:
            if (strcmp(node->text, "not") == 0) {
                fputc('!', cg->out);
            } else {
                fputs(node->text, cg->out);
            }
            emit_expr(cg, node->left, p, 0, node->text);
            break;

        default:
            break;
    }

    if (need_parens) {
        fputc(')', cg->out);
    }
}

/*
 * Gera uma lista de comandos (um bloco). Cada comando comeca com a indentacao atual.
 * Blocos aninhados aumentam `indent_level` antes de chamar emit_list recursivamente.
 */
static void emit_list(CodeGen *cg, const Node *node) {
    for (; node; node = node->next) {
        if (node->type == N_LIST) {
            emit_list(cg, node->body);
            continue;
        }

        emit_indent(cg);

        switch (node->type) {
            /*
             * Atribuicao: usa `let` apenas na PRIMEIRA vez que o nome aparece; depois, so `x = ...;`.
             * (Variaveis criadas dentro de blocos ja foram declaradas no topo por codegen_emit.)
             */
            case N_ASSIGN:
                if (!is_declared(cg, node->text)) {
                    fputs("let ", cg->out);
                    mark_declared(cg, node->text);
                }
                emit_ident(cg->out, node->text);
                fputs(" = ", cg->out);
                emit_expr(cg, node->left, 0, 0, NULL);
                fputs(";\n", cg->out);
                break;

            /* print(x) vira console.log(x); */
            case N_PRINT:
                fputs("console.log(", cg->out);
                emit_expr(cg, node->left, 0, 0, NULL);
                fputs(");\n", cg->out);
                break;

            /* if/else: `if (cond) { ... } else { ... }`, com o corpo um nivel mais indentado. */
            case N_IF:
                fputs("if (", cg->out);
                emit_expr(cg, node->left, 0, 0, NULL);
                fputs(") {\n", cg->out);

                cg->indent_level++;
                emit_list(cg, node->body);
                cg->indent_level--;

                emit_indent(cg);
                fputc('}', cg->out);

                if (node->else_body) {
                    fputs(" else {\n", cg->out);

                    cg->indent_level++;
                    emit_list(cg, node->else_body);
                    cg->indent_level--;

                    emit_indent(cg);
                    fputs("}\n", cg->out);
                } else {
                    fputc('\n', cg->out);
                }
                break;

            /* while: `while (cond) { ... }`. */
            case N_WHILE:
                fputs("while (", cg->out);
                emit_expr(cg, node->left, 0, 0, NULL);
                fputs(") {\n", cg->out);

                cg->indent_level++;
                emit_list(cg, node->body);
                cg->indent_level--;

                emit_indent(cg);
                fputs("}\n", cg->out);
                break;

            default:
                break;
        }
    }
}

/*
 * Abre o .js e gera o codigo em duas etapas:
 *   1) declara no topo (`let x;`) as variaveis cuja 1a atribuicao ocorreu dentro de um bloco
 *   2) percorre a AST emitindo os comandos
 */
int codegen_emit(const Node *root, const SymbolTable *symtab, const char *output_path) {
    FILE *out = fopen(output_path, "w");
    if (!out) {
        perror("Erro ao criar arquivo JavaScript");
        return 1;
    }

    /* Inicializa o gerador: arquivo, indentacao 0 e lista de declaradas vazia. */
    CodeGen cg = {out, 0, NULL, 0, 0};

    /* Variaveis cuja primeira atribuicao ocorre dentro de blocos (if/else/while)
     * sao declaradas no escopo global JS para preservar a semantica de escopo unico. */
    if (symtab) {
        for (const Symbol *sym = symtab->head; sym; sym = sym->next) {
            if (sym->decl_depth > 0 && !is_declared(&cg, sym->name)) {
                fputs("let ", out);
                emit_ident(out, sym->name);
                fputs(";\n", out);
                mark_declared(&cg, sym->name);
            }
        }
    }

    const Node *statements = (root && root->type == N_LIST) ? root->body : root;
    emit_list(&cg, statements);

    free(cg.declared);
    if (fclose(out) != 0) {
        perror("Erro ao fechar arquivo JavaScript");
        return 1;
    }

    return 0;
}
