CC = gcc
CFLAGS = -Wall -Wextra -g

all: tojs

parser.tab.c parser.tab.h: parser.y ast.h
	bison -d -o parser.tab.c parser.y

lex.yy.c: lexer.l parser.tab.h
	flex -o lex.yy.c lexer.l

tojs: parser.tab.c lex.yy.c ast.c main.c ast.h
	$(CC) $(CFLAGS) -o tojs parser.tab.c lex.yy.c ast.c main.c

clean:
	rm -f tojs tojs.exe parser.tab.c parser.tab.h lex.yy.c saida.js
