/*
 * lextest.c
 *
 * Harness que testa SO o analisador lexico: le o arquivo (ou stdin),
 * chama yylex() em laco e imprime um token por linha:
 *
 *     linha  TOKEN             lexema
 *
 * Nao linka o parser (parser.tab.c), entao define aqui o yylval que o
 * lexer espera encontrar. Retorna 0 se nao houve erro lexico, 1 caso
 * contrario (mesma convencao de main.c).
 */

#include <stdio.h>
#include <stdlib.h>
#include "parser.tab.h"

int yylex(void);
extern FILE *yyin;
extern char *yytext;
extern int yylineno;
extern int had_lexical_error;

/* Normalmente definido em parser.tab.c; aqui o parser nao e linkado. */
YYSTYPE yylval;

static const char *token_name(int t) {
    switch (t) {
    case PUBLIC: return "PUBLIC";
    case STATIC: return "STATIC";
    case CLASS: return "CLASS";
    case INT: return "INT";
    case FLOAT: return "FLOAT";
    case DOUBLE: return "DOUBLE";
    case BOOLEAN: return "BOOLEAN";
    case CHAR: return "CHAR";
    case VOID: return "VOID";
    case STRING_TYPE: return "STRING_TYPE";
    case IF: return "IF";
    case ELSE: return "ELSE";
    case WHILE: return "WHILE";
    case FOR: return "FOR";
    case DO: return "DO";
    case BREAK: return "BREAK";
    case CONTINUE: return "CONTINUE";
    case RETURN: return "RETURN";
    case IDENTIFIER: return "IDENTIFIER";
    case INTEGER_LITERAL: return "INTEGER_LITERAL";
    case REAL_LITERAL: return "REAL_LITERAL";
    case CHAR_LITERAL: return "CHAR_LITERAL";
    case STRING_LITERAL: return "STRING_LITERAL";
    case TRUE: return "TRUE";
    case FALSE: return "FALSE";
    case PLUS: return "PLUS";
    case MINUS: return "MINUS";
    case MULTIPLY: return "MULTIPLY";
    case DIVIDE: return "DIVIDE";
    case MODULO: return "MODULO";
    case EQUAL: return "EQUAL";
    case NOT_EQUAL: return "NOT_EQUAL";
    case LESS: return "LESS";
    case GREATER: return "GREATER";
    case LESS_EQUAL: return "LESS_EQUAL";
    case GREATER_EQUAL: return "GREATER_EQUAL";
    case AND: return "AND";
    case OR: return "OR";
    case NOT: return "NOT";
    case ASSIGN: return "ASSIGN";
    case PLUS_ASSIGN: return "PLUS_ASSIGN";
    case MINUS_ASSIGN: return "MINUS_ASSIGN";
    case MULT_ASSIGN: return "MULT_ASSIGN";
    case DIV_ASSIGN: return "DIV_ASSIGN";
    case INCREMENT: return "INCREMENT";
    case DECREMENT: return "DECREMENT";
    case LPAREN: return "LPAREN";
    case RPAREN: return "RPAREN";
    case LBRACE: return "LBRACE";
    case RBRACE: return "RBRACE";
    case LBRACKET: return "LBRACKET";
    case RBRACKET: return "RBRACKET";
    case SEMICOLON: return "SEMICOLON";
    case COMMA: return "COMMA";
    case DOT: return "DOT";
    default: return "(desconhecido)";
    }
}

int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            fprintf(stderr, "Nao foi possivel abrir o arquivo: %s\n", argv[1]);
            return 2;
        }
    }

    int t;
    while ((t = yylex()) != 0) {
        printf("%4d  %-16s %s\n", yylineno, token_name(t), yytext);
    }

    if (argc > 1) fclose(yyin);
    return had_lexical_error ? 1 : 0;
}