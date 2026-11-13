#include "ast.h"
#include <stdlib.h>
#include <string.h>

static char *ast_duplicate(const char *text) {
    const char *src = text ? text : "";
    size_t len = strlen(src) + 1;
    char *copy = malloc(len);

    if (!copy) {
        fprintf(stderr, "Erro: memoria insuficiente ao alocar texto da AST.\n");
        exit(1);
    }

    memcpy(copy, src, len);
    return copy;
}

static char *join_prefix(const char *prefix, const char *segment) {
    size_t p_len = strlen(prefix);
    size_t s_len = strlen(segment);
    char *joined = malloc(p_len + s_len + 1);

    if (!joined) {
        fprintf(stderr, "Erro: memoria insuficiente ao formatar AST.\n");
        exit(1);
    }

    memcpy(joined, prefix, p_len);
    memcpy(joined + p_len, segment, s_len + 1);
    return joined;
}

Node *node_new(NodeType type, int line) {
    Node *node = calloc(1, sizeof(Node));

    if (!node) {
        fprintf(stderr, "Erro: memoria insuficiente ao criar no da AST.\n");
        exit(1);
    }

    node->type = type;
    node->line = line;
    return node;
}

Node *node_list(Node *items, int line) {
    int start_line = items ? items->line : line;
    Node *node = node_new(N_LIST, start_line > 0 ? start_line : 1);
    node->body = items;
    return node;
}

Node *node_text(NodeType type, const char *text, int line) {
    Node *node = node_new(type, line);
    node->text = ast_duplicate(text);
    return node;
}

Node *node_number(NodeType type, double value, const char *text, int line) {
    Node *node = node_text(type, text, line);
    node->number = value;
    if (type == N_INT) {
        node->int_value = text ? strtoll(text, NULL, 10) : (long long)value;
        node->number = (double)node->int_value;
    } else if (text) {
        node->number = strtod(text, NULL);
    }
    return node;
}

Node *node_bool(int value, int line) {
    Node *node = node_text(N_BOOL, value ? "true" : "false", line);
    node->boolean = value ? 1 : 0;
    return node;
}

Node *node_binary(const char *op, Node *left, Node *right, int line) {
    Node *node = node_text(N_BINARY, op, line);
    node->left = left;
    node->right = right;
    return node;
}

Node *node_unary(const char *op, Node *operand, int line) {
    Node *node = node_text(N_UNARY, op, line);
    node->left = operand;
    return node;
}

Node *node_append(Node *list, Node *item) {
    if (!item) {
        return list;
    }
    if (!list) {
        return item;
    }

    Node *current = list;
    while (current->next) {
        current = current->next;
    }
    current->next = item;
    return list;
}

const char *node_type_name(NodeType type) {
    switch (type) {
        case N_LIST:   return "Program";
        case N_ASSIGN: return "Assign";
        case N_PRINT:  return "Print";
        case N_IF:     return "If";
        case N_WHILE:  return "While";
        case N_INT:    return "Int";
        case N_REAL:   return "Real";
        case N_STRING: return "String";
        case N_BOOL:   return "Bool";
        case N_NONE:   return "None";
        case N_ID:     return "Id";
        case N_BINARY: return "BinaryOp";
        case N_UNARY:  return "UnaryOp";
        default:       return "Unknown";
    }
}

int ast_count_nodes(const Node *node) {
    int count = 0;
    while (node) {
        count += 1;
        count += ast_count_nodes(node->left);
        count += ast_count_nodes(node->right);
        count += ast_count_nodes(node->body);
        count += ast_count_nodes(node->else_body);
        node = node->next;
    }
    return count;
}

static void ast_print_node(const Node *node, FILE *out, const char *prefix, int is_last);

static void ast_print_list(const Node *head, FILE *out, const char *prefix) {
    if (!head) {
        fprintf(out, "%s`-- (vazio)\n", prefix);
        return;
    }

    const Node *curr = head;
    while (curr) {
        ast_print_node(curr, out, prefix, curr->next == NULL);
        curr = curr->next;
    }
}

static void ast_print_node(const Node *node, FILE *out, const char *prefix, int is_last) {
    if (!node) {
        return;
    }

    const char *connector = is_last ? "`-- " : "|-- ";
    char *child_prefix = join_prefix(prefix, is_last ? "    " : "|   ");

    fprintf(out, "%s%s", prefix, connector);

    switch (node->type) {
        case N_LIST:
            fprintf(out, "Program (linha %d)\n", node->line);
            ast_print_list(node->body, out, child_prefix);
            break;

        case N_ASSIGN:
            fprintf(out, "Assign '%s' (linha %d)\n",
                    node->text ? node->text : "", node->line);
            ast_print_node(node->left, out, child_prefix, 1);
            break;

        case N_PRINT:
            fprintf(out, "Print (linha %d)\n", node->line);
            ast_print_node(node->left, out, child_prefix, 1);
            break;

        case N_IF: {
            fprintf(out, "If (linha %d)\n", node->line);

            fprintf(out, "%s|-- Condicao:\n", child_prefix);
            char *cond_prefix = join_prefix(child_prefix, "|   ");
            ast_print_node(node->left, out, cond_prefix, 1);
            free(cond_prefix);

            int has_else = (node->else_body != NULL);
            fprintf(out, "%s%sBloco Then:\n", child_prefix, has_else ? "|-- " : "`-- ");
            char *then_prefix = join_prefix(child_prefix, has_else ? "|   " : "    ");
            ast_print_list(node->body, out, then_prefix);
            free(then_prefix);

            if (has_else) {
                fprintf(out, "%s`-- Bloco Else:\n", child_prefix);
                char *else_prefix = join_prefix(child_prefix, "    ");
                ast_print_list(node->else_body, out, else_prefix);
                free(else_prefix);
            }
            break;
        }

        case N_WHILE: {
            fprintf(out, "While (linha %d)\n", node->line);

            fprintf(out, "%s|-- Condicao:\n", child_prefix);
            char *cond_prefix = join_prefix(child_prefix, "|   ");
            ast_print_node(node->left, out, cond_prefix, 1);
            free(cond_prefix);

            fprintf(out, "%s`-- Bloco:\n", child_prefix);
            char *body_prefix = join_prefix(child_prefix, "    ");
            ast_print_list(node->body, out, body_prefix);
            free(body_prefix);
            break;
        }

        case N_BINARY:
            fprintf(out, "BinaryOp '%s' (linha %d)\n",
                    node->text ? node->text : "", node->line);
            ast_print_node(node->left, out, child_prefix, 0);
            ast_print_node(node->right, out, child_prefix, 1);
            break;

        case N_UNARY:
            fprintf(out, "UnaryOp '%s' (linha %d)\n",
                    node->text ? node->text : "", node->line);
            ast_print_node(node->left, out, child_prefix, 1);
            break;

        case N_INT:
            fprintf(out, "Int %s (linha %d)\n",
                    node->text ? node->text : "0", node->line);
            break;

        case N_REAL:
            fprintf(out, "Real %s (linha %d)\n",
                    node->text ? node->text : "0.0", node->line);
            break;

        case N_STRING:
            fprintf(out, "String %s (linha %d)\n",
                    node->text ? node->text : "\"\"", node->line);
            break;

        case N_BOOL:
            fprintf(out, "Bool %s (linha %d)\n",
                    node->boolean ? "True" : "False", node->line);
            break;

        case N_NONE:
            fprintf(out, "None (linha %d)\n", node->line);
            break;

        case N_ID:
            fprintf(out, "Id '%s' (linha %d)\n",
                    node->text ? node->text : "", node->line);
            break;

        default:
            fprintf(out, "Unknown (linha %d)\n", node->line);
            break;
    }

    free(child_prefix);
}

void ast_print(const Node *node, FILE *out) {
    if (!out) {
        out = stdout;
    }

    fprintf(out, "=== Arvore Sintatica Abstrata (AST) ===\n");
    if (!node) {
        fprintf(out, "Program (vazio)\n");
        return;
    }

    if (node->type == N_LIST) {
        fprintf(out, "Program (linha %d, %d no(s))\n",
                node->line, ast_count_nodes(node));
        ast_print_list(node->body, out, "");
    } else {
        ast_print_list(node, out, "");
    }
}

void ast_free(Node *node) {
    while (node) {
        Node *next = node->next;

        ast_free(node->left);
        ast_free(node->right);
        ast_free(node->body);
        ast_free(node->else_body);

        free(node->text);
        free(node);

        node = next;
    }
}
