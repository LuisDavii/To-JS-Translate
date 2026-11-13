
%option noyywrap yylineno noinput nounput

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

static int indent_stack[256] = {0};
static int indent_top = 0;
static int pending_dedents = 0;
static int line_has_token = 0;
static int eof_newline_sent = 0;

#define YY_DECL static int lexer_next(void)
#define YY_USER_INIT BEGIN(BOL);
#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yylloc.last_column = 1;

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

DIGIT       [0-9]
ID_START    [a-zA-Z_]
ID_CONT     [a-zA-Z0-9_]
INTEGER     {DIGIT}+
REALNUM     {DIGIT}+\.{DIGIT}+
STRING_DQ   \"([^\\\"\n]|\\.)*\"
STRING_SQ   \'([^\\\'\n]|\\.)*\'

%x BOL

%%

<BOL>[ \t]+#[^\r\n]* {
}

<BOL>#[^\r\n]* {
}

<BOL>[ \t]+\r?\n {
}

<BOL>\r?\n {
}

<BOL>[ \t]+ {
    int col = 0;

    for (int i = 0; i < yyleng; i++) {
        col += (yytext[i] == '\t') ? 4 : 1;
    }

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

    if (col < indent_stack[indent_top]) {
        int count = 0;

        while (indent_top > 0 &&
               col < indent_stack[indent_top]) {
            indent_top--;
            count++;
        }

        if (col != indent_stack[indent_top]) {
            fprintf(stderr,
                    "Erro lexico na linha %d: indentacao inconsistente\n",
                    yylineno);
            exit(1);
        }

        pending_dedents = count - 1;
        BEGIN(INITIAL);
        return DEDENT;
    }

    BEGIN(INITIAL);
}

<BOL>[^ \t\r\n#] {
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
}

<INITIAL>\r?\n {
    yylloc.first_line = yylloc.last_line = (yylineno > 1) ? yylineno - 1 : 1;
    BEGIN(BOL);
    return NEWLINE;
}

<INITIAL>[ \t\r]+ {
}

"if"       { return IF; }
"else"     { return ELSE; }
"while"    { return WHILE; }
"print"    { return PRINT; }

"True"     { return TRUE; }
"False"    { return FALSE; }
"None"     { return NONE; }

"and"      { return AND; }
"or"       { return OR; }
"not"      { return NOT; }

"=="       { return EQ; }
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
    yylval.text = copy_text(yytext);
    return IDENT;
}

. {
    fprintf(stderr,
            "Erro lexico na linha %d: caractere inesperado '%s'\n",
            yylineno, yytext);
    exit(1);
}

<INITIAL,BOL><<EOF>> {
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
