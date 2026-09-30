/*
 * lexer.l
 *
 * Analisador lexico (Flex) do compilador Java -> C. Cobre todos os tokens
 * da Especificacao Lexica do subconjunto Java:
 *
 *   - palavras-chave (tipos, controle de fluxo, modificadores)
 *   - identificadores
 *   - literais: inteiro, real (com expoente e sufixo f/d), char, string,
 *     true/false
 *   - operadores aritmeticos, relacionais, logicos, atribuicao, ++/--
 *   - delimitadores, incluindo [ ] (necessarios para "String[] args")
 *   - comentarios de linha e de bloco
 *
 * Erros lexicos sao reportados em stderr e marcam had_lexical_error = 1.
 * O token invalido e descartado (nenhum token e retornado ao parser).
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

#include <errno.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <ctype.h>

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

/* Linha onde o comentario de bloco atual comecou (para erro de EOF).
   
   A análise precisa registrar a linha inicial do comentário atual para
   poder reportar um erro de EOF antes do fechamento do bloco('*barra'). 
*/
static int comment_start_line = 0;

static void lex_error(const char *fmt, const char *arg) {
    fprintf(stderr, "ERRO LEXICO na linha %d: ", yylineno);
    fprintf(stderr, fmt, arg);
    fputc('\n', stderr);
    had_lexical_error = 1;
}

/*
 * Converte yytext (literal real ja validado sintaticamente) e rejeita
 * valores fora do intervalo: overflow para double (1e999) ou, com sufixo
 * f/F, fora do intervalo de float (1e39f). Devolve 1 se o valor e valido.
 */
static int real_value_ok(void) {
    size_t n = strlen(yytext);
    int is_float = (yytext[n - 1] == 'f' || yytext[n - 1] == 'F');
    double d = strtod(yytext, NULL);
    if (isinf(d) || (is_float && fabs(d) > (double) FLT_MAX)) {
        lex_error("literal real '%s' fora do intervalo.", yytext);
        return 0;
    }
    yylval.dval = d;
    return 1;
}
%}

%option noyywrap
%option yylineno
%option nounput
%option noinput

/* Comentario de bloco tratado com start condition, para poder detectar
 * comentario nao terminado (EOF dentro do comentario).
 
 ao detectar '/barra', o lexer troca para um estado especial de comentário.
 Nesse estado, ele ignora quase tudo e só procura o fechamento do bloco.
 Se o fim do arquivo chegar antes de encontrar '*barra',
 o lexer pode identificar esse caso como erro de comentário não fechado. 
*/

%x BLOCK_COMMENT

DIGIT       [0-9]
LETTER      [a-zA-Z_]

EXP         [eE][+-]?{DIGIT}+
/* Escapes aceitos em char/string: iguais em Java e em C, entao a geracao de codigo pode copia-los sem traducao.
 o "regex" do EXP significa: e ou E, opcionalmente com sinal + ou -, seguido de um ou mais dígitos.
 */

ESC         \\[ntrbf0\\\'\"]
/*newline tab carriage return... tudo igual sem precisar de traducao*/

%%

 /* ---------- Tratamento de Comentarios ---------- */
 /*
 Respectivamente
 ignora comentário de linha.
 inicia comentário de bloco e guarda a linha inicial.
 fecha o bloco e volta ao estado normal.
 consome conteúdo do bloco sem interferir.
 ignora asterisco solto dentro do bloco.
 detecta fim de arquivo sem fechar o bloco e imprime: ERRO LEXICO na linha X: comentario de bloco nao terminado.
 */

"//".*                          { /* comentario de linha: ignorado */ } 
"/*"                            { comment_start_line = yylineno; BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/"             { BEGIN(INITIAL); }
<BLOCK_COMMENT>[^*]+            { /* consome; yylineno conta os \n */ }
<BLOCK_COMMENT>"*"              { /* asterisco solto dentro do comentario */ }
<BLOCK_COMMENT><<EOF>>          {
    fprintf(stderr, "ERRO LEXICO na linha %d: comentario de bloco nao terminado.\n", comment_start_line);
    had_lexical_error = 1;
    BEGIN(INITIAL);
    return 0;
}


 /* ---------- Espacos em branco ---------- */
[ \t\r\n\f]+                     { /* espacos em branco: ignorados */ }


 /* ---------- Palavras-chave ---------- */

"public"        { return PUBLIC; }
"static"        { return STATIC; }
"class"         { return CLASS; }
"int"           { return INT; }
"float"         { return FLOAT; }
"double"        { return DOUBLE; }
"boolean"       { return BOOLEAN; }
"char"          { return CHAR; }
"void"          { return VOID; }
"String"        { return STRING_TYPE; }  
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

 /* ---------- Literais numericos ----------
   Ordem importa: quando duas regras casam o mesmo tamanho, vence a primeira listada.
   Por isso as regras validas vem antes da regra de "numero malformado" (que casa
   qualquer digito seguido de letras). 

   A ideia é: validar o token certo primeiro, e só depois cair nos erros mais específicos.

    yytext guarda o texto lido
    atof(yytext) converte para double
    return REAL_LITERAL; devolve o token para o parser
*/

 /* número com parte decimal, como 3.14, 2.0, 5.5e10 e opcionalmente aceita exponente e sufixo f, F, d, D */
{DIGIT}+"."{DIGIT}+({EXP})?[fFdD]? {
    if (real_value_ok()) return REAL_LITERAL;
}

 /* número sem ponto, mas com expoente, como 10e3, 7E-2,também aceita sufixo f/d */
{DIGIT}+{EXP}[fFdD]? {
    if (real_value_ok()) return REAL_LITERAL;
}
 /* número inteiro com sufixo de float/double, como 10f, 25d */
{DIGIT}+[fFdD] {
    if (real_value_ok()) return REAL_LITERAL;
}

{DIGIT}+ {
    /* Nota: Java aceita 2147483648 apenas como operando do menos unario
     * (-2147483648). Aqui rejeitamos qualquer valor > INT_MAX; limitacao
     * aceitavel para o subconjunto. */
    long long v;
    errno = 0;
    v = strtoll(yytext, NULL, 10);
    if (errno == ERANGE || v > INT_MAX) {
        lex_error("literal inteiro '%s' fora do intervalo de int.", yytext);
    } else {
        yylval.ival = (int) v;
        return INTEGER_LITERAL;
    }
}

{DIGIT}({LETTER}|{DIGIT})* {
    lex_error("literal numerico malformado '%s'.", yytext);
}

 /* Reais malformados: sem estas regras, "1.5e" viraria REAL(1.5) + IDENT(e)
    e "1.5abc" viraria REAL + IDENT, sem nenhum erro lexico. Empates de
    tamanho com as regras validas sao resolvidos a favor delas (vem antes). */
{DIGIT}+"."{DIGIT}+({LETTER}|{DIGIT})* {
    lex_error("literal numerico malformado '%s'.", yytext);
}

{DIGIT}+("."{DIGIT}+)?[eE][+-] {
    lex_error("literal numerico malformado '%s' (expoente sem digitos).", yytext);
}

  /* ---------- Literais de caractere ----------
  * As regras de erro capturam casos malformados com UMA mensagem clara,
  * em vez de deixar o catch-all fragmentar o literal em varios erros de
  * caractere isolado. Regras validas primeiro, para vencer empates de
  * tamanho contra as regras de erro. */

 /* caractere válido simples, tira aspas e retorna char_literal */
\'[^\'\\\n]\' {
    yylval.sval = strdup(yytext + 1);
    yylval.sval[1] = '\0';
    return CHAR_LITERAL;
}

\'{ESC}\' {
    /* sval guarda o escape cru (ex.: \n), pronto para ser copiado no C. */
    yylval.sval = strdup(yytext + 1);
    yylval.sval[2] = '\0';
    return CHAR_LITERAL;
}

\'\\.\' {
    lex_error("escape em literal de caractere invalido: %s", yytext);
}

\'\' {
    lex_error("literal de caractere vazio%s.", "");
}

\'[^\'\n]*\' {
    lex_error("literal de caractere invalido %s (deve conter exatamente um caractere ASCII).", yytext);
}

\'[^\'\n]* {
    lex_error("literal de caractere nao terminado%s.", "");
}

 /* ---------- Literais de string ----------
  * Aceita qualquer sequencia \x (inclusive \") sem validar o escape; o
  * conteudo e guardado cru, sem as aspas. */

\"([^\"\\\n]|{ESC})*\" {
    yylval.sval = strdup(yytext + 1);
    yylval.sval[strlen(yylval.sval) - 1] = '\0';
    return STRING_LITERAL;
}

 /* String bem fechada, mas com escape fora de {ESC} (ex.: "a\qb"). Empata em
    tamanho com a regra valida, que vem antes e vence quando tudo e valido. */
\"([^\"\\\n]|\\.)*\" {
    lex_error("escape invalido em string literal: %s", yytext);
}

 /* O \\? final absorve uma barra solta no fim da linha, evitando um
    segundo erro "caractere '\\' nao reconhecido". */
\"([^\"\\\n]|\\.)*\\? {
    lex_error("string literal nao terminada%s.", "");
}

 /* ---------- Identificadores ---------- */

{LETTER}({LETTER}|{DIGIT})* {
    yylval.sval = strdup(yytext);
    return IDENTIFIER;
}

 /* ---------- Operadores ---------- */

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

 /* ---------- Delimitadores ---------- */

"("             { return LPAREN; }
")"             { return RPAREN; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
"["             { return LBRACKET; }
"]"             { return RBRACKET; }
";"             { return SEMICOLON; }
","             { return COMMA; }
"."             { return DOT; }

 /* ---------- Qualquer outra coisa ---------- */

 /* BOM UTF-8 no inicio de uma linha (arquivos salvos pelo Windows) */
^\xEF\xBB\xBF                  { /* ignorado */ }

 /* Bytes nao-ASCII fora de string/char/comentario: um unico erro por
    sequencia, em vez de um erro (ilegivel) por byte. */
[\x80-\xFF]+ {
    lex_error("caractere(s) nao-ASCII '%s' nao reconhecido(s) fora de literal ou comentario.", yytext);
}

. {
    if (isprint((unsigned char) yytext[0])) {
        lex_error("caractere '%s' nao reconhecido.", yytext);
    } else {
        char hex[8];
        snprintf(hex, sizeof hex, "0x%02X", (unsigned char) yytext[0]);
        lex_error("caractere de controle %s nao reconhecido.", hex);
    }
}

%%