
%option noyywrap yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

static int indent_stack[256] = {0};
static int indent_top = 0;
static int pending_dedents = 0;

#define YY_DECL static int lexer_next(void)

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

<BOL>[ \t]+ {
    int col = 0;

    for (int i = 0; i < yyleng; i++) {
        col += (yytext[i] == '\t') ? 4 : 1;
    }

    if (col > indent_stack[indent_top]) {
        if (indent_top >= 255) {
            fprintf(stderr, "Erro: indentacao muito profunda na linha %d\n", yylineno);
            exit(1);
        }

        indent_stack[++indent_top] = col;
        BEGIN(INITIAL);
        return INDENT;
    }

    if (col < indent_stack[indent_top]) {
        int count = 0;

        while (indent_top > 0 && col < indent_stack[indent_top]) {
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

<BOL>#[^\n]* {
    /* Ignora comentarios */
}

<BOL>\n {
    /* Ignora linhas vazias */
}

<BOL>[^ \t\n#] {
    BEGIN(INITIAL);
    yyless(0);
}

<INITIAL>#[^\n]* {
    /* Ignora comentarios */
}

<INITIAL>\n {
    BEGIN(BOL);
    return NEWLINE;
}

[ \t\r]+ {
    /* Ignora espacos entre tokens */
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
    if (indent_top > 0) {
        indent_top--;
        return DEDENT;
    }

    return 0;
}

%%

int yylex(void) {
    if (pending_dedents > 0) {
        pending_dedents--;
        return DEDENT;
    }

    return lexer_next();
}
