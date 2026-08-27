/*
 * lexer.l
 *
 * Prototipo minimo do analisador lexico (Flex) para o compilador
 * Java -> C. Implementa apenas o subconjunto de tokens necessario
 * para o parser de prototipo (parser.y), seguindo a Especificacao
 * Lexica do subconjunto Java.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

/*
 * Flag global que sinaliza se algum erro lexico ocorreu durante a
 * analise. E essencial checar isso em main.c apos yyparse(), porque o
 * parser pode terminar "com sucesso" (yyparse() == 0) mesmo que o lexer
 * tenha reportado um ou mais erros no meio do caminho — por exemplo,
 * quando o token invalido acaba sendo simplesmente descartado e o
 * restante da entrada ainda forma uma sentenca sintaticamente valida.
 * Sem essa flag, o compilador acabaria gerando (ou fingindo que geraria)
 * codigo C para um programa que na verdade tem erro lexico, violando a
 * regra da especificacao de que nenhum erro lexico pode passar batido.
 */
int had_lexical_error = 0;
%}

%option noyywrap
%option yylineno

DIGIT       [0-9]
LETTER      [a-zA-Z_]

%%

"//".*                          { /* comentario de linha: ignorado */ }
"/*"([^*]|\*+[^*/])*\*+"/"      { /* comentario de bloco: ignorado */ }

[ \t\r\n]+                       { /* espacos em branco: ignorados */ }

"public"        { return PUBLIC; }
"static"        { return STATIC; }
"class"         { return CLASS; }
"int"           { return INT; }
"float"         { return FLOAT; }
"double"        { return DOUBLE; }
"boolean"       { return BOOLEAN; }
"char"          { return CHAR; }
"void"          { return VOID; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"for"           { return FOR; }
"do"            { return DO; }
"break"         { return BREAK; }
"continue"      { return CONTINUE; }
"return"        { return RETURN; }
"true"          { return TRUE; }
"false"         { return FALSE; }

{DIGIT}+"."{DIGIT}+ {
    yylval.dval = atof(yytext);
    return REAL_LITERAL;
}

{DIGIT}+ {
    yylval.ival = atoi(yytext);
    return INTEGER_LITERAL;
}

\'.\' {
    yylval.sval = strdup(yytext + 1);
    yylval.sval[1] = '\0';
    return CHAR_LITERAL;
}

    /*
     * As regras abaixo capturam casos malformados de literal de caractere
     * com UMA mensagem de erro clara, em vez de deixar o catch-all
     * genérico fragmentar o literal em vários erros de caractere isolado
     * (o que acontecia antes, ex.: '\n' virava 3 erros distintos para
     * ', \ e ' separadamente). A ordem importa: o flex usa a regra que
     * casa o maior trecho de entrada: as regras mais especificas (escape,
     * vazio) empatam em tamanho com a regra generica de "varios
     * caracteres" para alguns casos, entao sao listadas primeiro para
     * vencer o empate.
     */

\'\\.\' {
    fprintf(stderr, "ERRO LEXICO na linha %d: escape '%s' em literal de caractere nao suportado nesta versao.\n", yylineno, yytext);
    had_lexical_error = 1;
}

\'\' {
    fprintf(stderr, "ERRO LEXICO na linha %d: literal de caractere vazio.\n", yylineno);
    had_lexical_error = 1;
}

\'[^\'\n]*\' {
    fprintf(stderr, "ERRO LEXICO na linha %d: literal de caractere invalido '%s' (deve conter exatamente um caractere).\n", yylineno, yytext);
    had_lexical_error = 1;
}

\'[^\'\n]* {
    fprintf(stderr, "ERRO LEXICO na linha %d: literal de caractere nao terminado.\n", yylineno);
    had_lexical_error = 1;
}

\"[^\"\n]*\" {
    yylval.sval = strdup(yytext + 1);
    yylval.sval[strlen(yylval.sval) - 1] = '\0';
    return STRING_LITERAL;
}

\"[^\"\n]* {
    fprintf(stderr, "ERRO LEXICO na linha %d: string literal nao terminada.\n", yylineno);
    had_lexical_error = 1;
}

{LETTER}({LETTER}|{DIGIT})* {
    yylval.sval = strdup(yytext);
    return IDENTIFIER;
}

"=="            { return EQUAL; }
"!="            { return NOT_EQUAL; }
"<="            { return LESS_EQUAL; }
">="            { return GREATER_EQUAL; }
"&&"            { return AND; }
"||"            { return OR; }
"+="            { return PLUS_ASSIGN; }
"-="            { return MINUS_ASSIGN; }
"*="            { return MULT_ASSIGN; }
"/="            { return DIV_ASSIGN; }
"++"            { return INCREMENT; }
"--"            { return DECREMENT; }

"<"             { return LESS; }
">"             { return GREATER; }
"!"             { return NOT; }
"="             { return ASSIGN; }
"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return MULTIPLY; }
"/"             { return DIVIDE; }
"%"             { return MODULO; }

"("             { return LPAREN; }
")"             { return RPAREN; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
";"             { return SEMICOLON; }
","             { return COMMA; }
"."             { return DOT; }

. {
    fprintf(stderr, "ERRO LEXICO na linha %d: caractere '%s' nao reconhecido.\n", yylineno, yytext);
    had_lexical_error = 1;
}

%%
