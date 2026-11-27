
/*
 * lexer.l - ANALISADOR LEXICO (Flex) do PtoJS.
 *
 * Transforma o texto do .tojs numa sequencia de TOKENS para o parser (parser.y).
 * Alem dos tokens normais, simula a indentacao do Python gerando tokens
 * virtuais: NEWLINE (fim de linha), INDENT (abre bloco) e DEDENT (fecha bloco).
 *
 * Opcoes: noyywrap (nao encadeia varios arquivos), yylineno (Flex mantem o
 * numero da linha atual), noinput/nounput (nao gera funcoes nao usadas).
 */
%option noyywrap yylineno noinput nounput

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

/* Pilha de niveis de indentacao (em colunas). O fundo e sempre 0 (sem recuo). */
static int indent_stack[256] = {0};
/* Indice do topo da pilha: 0 = fora de qualquer bloco. */
static int indent_top = 0;
/* DEDENTs que ainda faltam entregar: uma acao do Flex devolve so UM token,
   mas fechar varios blocos de uma vez exige varios DEDENT seguidos. */
static int pending_dedents = 0;
/* 1 se a linha atual ja teve algum token (para saber se falta um NEWLINE no fim do arquivo). */
static int line_has_token = 0;
/* 1 depois que o NEWLINE sintetico do fim de arquivo foi enviado (evita enviar duas vezes). */
static int eof_newline_sent = 0;

/* Renomeia a funcao de varredura gerada pelo Flex para lexer_next(). O yylex()
   "de verdade" fica no fim do arquivo e a envolve para injetar os DEDENTs pendentes. */
#define YY_DECL static int lexer_next(void)
/* Ao iniciar, o lexer entra no estado BOL (comeco de linha) para medir a 1a indentacao. */
#define YY_USER_INIT BEGIN(BOL);
/* Executada antes de TODA acao: grava a linha atual em yylloc, que o Bison usa
   nas mensagens de erro (@1.first_line). A coluna nao e rastreada (fixa em 1). */
#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yylloc.last_column = 1;

/* Copia o lexema para o heap. Necessario porque yytext e sobrescrito a cada token,
   e o parser precisa guardar o texto (nome de variavel, numero, string).
   O parser e quem libera essa memoria (free). */
static char *copy_text(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);

    if (!p) {
        fprintf(stderr, "Erro: memoria insuficiente\n");
        exit(1);
    }

    memcpy(p, s, n);
    return p;
}
%}

/* Definicoes (apelidos) de expressoes regulares usados nas regras abaixo. */
DIGIT       [0-9]
ID_START    [a-zA-Z_]
ID_CONT     [a-zA-Z0-9_]
INTEGER     {DIGIT}+
REALNUM     {DIGIT}+\.{DIGIT}+
/* String entre aspas duplas ou simples: aceita escapes (\x) e NAO atravessa
   quebra de linha. String nao fechada nao casa e cai na regra de erro ".". */
STRING_DQ   \"([^\\\"\n]|\\.)*\"
STRING_SQ   \'([^\\\'\n]|\\.)*\'

/* Estado EXCLUSIVO BOL (Beginning Of Line): so as regras marcadas <BOL> valem nele.
   E nele que se mede a indentacao e se ignoram linhas vazias/comentarios. */
%x BOL

%%

<BOL>[ \t]+#[^\r\n]* {
    /* Linha so com comentario (indentada): ignora, sem alterar a pilha de indentacao. */
}

<BOL>#[^\r\n]* {
    /* Linha so com comentario (sem recuo): ignora. */
}

<BOL>[ \t]+\r?\n {
    /* Linha com so espacos/tabs: linha em branco, ignorada (nao gera NEWLINE nem INDENT). */
}

<BOL>\r?\n {
    /* Linha totalmente vazia: ignorada. */
}

<BOL>[ \t]+ {
    /* Espacos no comeco de uma linha COM codigo: aqui se decide INDENT/DEDENT.
     * Passo 1: calcula a coluna (tab conta como 4 espacos). */
    int col = 0;

    for (int i = 0; i < yyleng; i++) {
        col += (yytext[i] == '\t') ? 4 : 1;
    }

    /* Mais recuo que o bloco atual: abre um bloco novo -> empilha e devolve INDENT. */
    if (col > indent_stack[indent_top]) {
        if (indent_top >= 255) {
            fprintf(stderr,
                    "Erro: indentacao muito profunda na linha %d\n",
                    yylineno);
            exit(1);
        }

        indent_stack[++indent_top] = col;
        BEGIN(INITIAL);
        return INDENT;
    }

    /* Menos recuo: fecha blocos. Desempilha ate achar um nivel <= col, contando
     * quantos blocos foram fechados. */
    if (col < indent_stack[indent_top]) {
        int count = 0;

        while (indent_top > 0 &&
               col < indent_stack[indent_top]) {
            indent_top--;
            count++;
        }

        /* Se a coluna nao bate com nenhum nivel anterior (ex.: 4 -> 2 quando
         * os niveis eram 0 e 4), a indentacao e inconsistente: erro lexico. */
        if (col != indent_stack[indent_top]) {
            fprintf(stderr,
                    "Erro lexico na linha %d: indentacao inconsistente\n",
                    yylineno);
            exit(1);
        }

        /* Devolve 1 DEDENT agora; os `count - 1` restantes saem em yylex(). */
        pending_dedents = count - 1;
        BEGIN(INITIAL);
        return DEDENT;
    }

    /* Mesmo recuo do bloco atual: nada a fazer, so segue lendo a linha. */
    BEGIN(INITIAL);
}

<BOL>[^ \t\r\n#] {
    /* Linha com codigo que comeca na coluna 0 (sem recuo): se havia blocos abertos,
     * todos se fecham. yyless(0) "devolve" o caractere lido para ser relido
     * normalmente no estado INITIAL. */
    if (indent_top > 0) {
        int count = indent_top;
        indent_top = 0;
        pending_dedents = count - 1;

        BEGIN(INITIAL);
        yyless(0);

        return DEDENT;
    }

    BEGIN(INITIAL);
    yyless(0);
}

<INITIAL>#[^\r\n]* {
    /* Comentario no fim de uma linha de codigo: ignora ate o fim da linha. */
}

<INITIAL>\r?\n {
    /* Fim de linha: devolve NEWLINE e volta ao estado BOL para medir a proxima
     * indentacao. O Flex ja incrementou yylineno, por isso a linha reportada e -1. */
    yylloc.first_line = yylloc.last_line = (yylineno > 1) ? yylineno - 1 : 1;
    BEGIN(BOL);
    return NEWLINE;
}

<INITIAL>[ \t\r]+ {
    /* Espacos entre tokens: ignorados. */
}

"if"       { /* Palavras reservadas. Ficam ANTES da regra de identificador: o Flex escolhe
              * o MAIOR casamento e, em empate, a regra que aparece primeiro; assim "if"
              * vira IF, mas "ifx" (casamento maior) vira IDENT. */
             return IF; }
"else"     { return ELSE; }
"while"    { return WHILE; }
"print"    { return PRINT; }

"True"     { return TRUE; }
"False"    { return FALSE; }
"None"     { return NONE; }

"and"      { return AND; }
"or"       { return OR; }
"not"      { return NOT; }

"=="       { /* Operadores de 2 caracteres (==, !=, >=, <=) tem prioridade sobre os de 1
              * caractere (>, <, =) porque o casamento mais longo vence. Um "!" sozinho
              * nao casa com nada e cai na regra de erro. */
             return EQ; }
"!="       { return NE; }
">="       { return GE; }
"<="       { return LE; }
">"        { return GT; }
"<"        { return LT; }
"="        { return ASSIGN; }

"+"        { return PLUS; }
"-"        { return MINUS; }
"*"        { return STAR; }
"/"        { return SLASH; }
"%"        { return MOD; }

"("        { return LPAREN; }
")"        { return RPAREN; }
":"        { return COLON; }
","        { return COMMA; }
";"        { return FIM_COMANDO; }

{REALNUM} {
    /* Numeros e strings: o texto original e entregue ao parser em yylval.text. */
    yylval.text = copy_text(yytext);
    return REAL;
}

{INTEGER} {
    yylval.text = copy_text(yytext);
    return INTEGER;
}

{STRING_DQ}|{STRING_SQ} {
    yylval.text = copy_text(yytext);
    return STRING;
}

{ID_START}{ID_CONT}* {
    /* Identificador: letra ou _ seguido de letras, digitos ou _. */
    yylval.text = copy_text(yytext);
    return IDENT;
}

. {
    /* Qualquer outro caractere (@, $, ! sozinho, string sem fechar...): erro lexico
     * e o tradutor para imediatamente. */
    fprintf(stderr,
            "Erro lexico na linha %d: caractere inesperado '%s'\n",
            yylineno, yytext);
    exit(1);
}

<INITIAL,BOL><<EOF>> {
    /* Fim do arquivo. Em ordem: (1) se a ultima linha tinha codigo mas nao terminava
     * em \n, envia um NEWLINE sintetico; (2) fecha, um por vez, os blocos ainda
     * abertos com DEDENT; (3) por fim devolve 0, que sinaliza EOF ao parser. */
    yylloc.first_line = yylloc.last_line = yylineno;

    if (line_has_token && !eof_newline_sent) {
        line_has_token = 0;
        eof_newline_sent = 1;
        BEGIN(BOL);
        return NEWLINE;
    }

    if (indent_top > 0) {
        indent_top--;
        return DEDENT;
    }

    return 0;
}

%%

/* yylex: funcao que o Bison realmente chama. Envolve lexer_next() para:
 *  1) entregar primeiro os DEDENTs pendentes (um por chamada);
 *  2) atualizar line_has_token, usado no tratamento de fim de arquivo. */
int yylex(void) {
    int token;

    if (pending_dedents > 0) {
        pending_dedents--;
        yylloc.first_line = yylloc.last_line = yylineno;
        yylloc.first_column = yylloc.last_column = 1;
        return DEDENT;
    }

    token = lexer_next();

    if (token == NEWLINE) {
        line_has_token = 0;
    } else if (token != 0 &&
               token != INDENT &&
               token != DEDENT) {
        line_has_token = 1;
    }

    return token;
}
