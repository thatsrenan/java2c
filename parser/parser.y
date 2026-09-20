/*
 * parser.y
 *
 * Prototipo minimo do analisador sintatico (Bison) para o compilador
 * Java -> C. Cobre apenas um subconjunto pequeno da linguagem, suficiente
 * para validar a integracao entre Flex e Bison:
 *
 *   - declaracao de variavel (int, float, double, boolean, char, String)
 *   - atribuicao (=, +=, -=, *=, /=)
 *   - expressoes aritmeticas, relacionais e logicas com precedencia
 *   - if / else if / else
 *   - while, for, do-while, break, continue, return
 *   - incremento/decremento (i++, ++i, i--, --i)
 *   - blocos { ... }
 *   - construcao simplificada System.out.println(...)
 *
 * Nao implementa a linguagem completa (ver especificacao de escopo).
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yylex(void);
void yyerror(const char *s);
extern int yylineno;
%}

%union {
    int    ival;
    double dval;
    char   *sval;
}

/* Palavras-chave */
%token PUBLIC STATIC CLASS
%token INT FLOAT DOUBLE BOOLEAN CHAR VOID STRING_TYPE
%token IF ELSE WHILE FOR DO BREAK CONTINUE RETURN

/* Literais e identificadores */
%token <sval> IDENTIFIER
%token <ival> INTEGER_LITERAL
%token <dval> REAL_LITERAL
%token <sval> CHAR_LITERAL
%token <sval> STRING_LITERAL
%token TRUE FALSE

/* Operadores */
%token PLUS MINUS MULTIPLY DIVIDE MODULO
%token EQUAL NOT_EQUAL LESS GREATER LESS_EQUAL GREATER_EQUAL
%token AND OR NOT
%token ASSIGN PLUS_ASSIGN MINUS_ASSIGN MULT_ASSIGN DIV_ASSIGN
%token INCREMENT DECREMENT

/* Delimitadores */
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET SEMICOLON COMMA DOT

%type <sval> type assign_op
%type <ival> expr

/* Precedencia (da menor para a maior) */
%left OR
%left AND
%left EQUAL NOT_EQUAL
%left LESS GREATER LESS_EQUAL GREATER_EQUAL
%left PLUS MINUS
%left MULTIPLY DIVIDE MODULO
%right NOT UMINUS

%%

program:
        stmt_list
        {
            printf(">> Programa reconhecido com sucesso.\n");
        }
    ;

stmt_list:
        /* vazio */
    | stmt_list stmt
    ;

stmt:
        vardecl
    |   assign_stmt
    |   print_stmt
    |   if_stmt
    |   while_stmt
    |   for_stmt
    |   do_while_stmt
    |   break_stmt
    |   continue_stmt
    |   return_stmt
    |   increment_stmt
    |   block
    ;

block:
        LBRACE stmt_list RBRACE
    ;

type:
        INT          { $$ = "int"; }
    |   FLOAT        { $$ = "float"; }
    |   DOUBLE       { $$ = "double"; }
    |   BOOLEAN      { $$ = "boolean"; }
    |   CHAR         { $$ = "char"; }
    |   STRING_TYPE  { $$ = "String"; }
    ;

vardecl:
        type IDENTIFIER SEMICOLON
        {
            printf("   [decl]   tipo=%s nome=%s\n", $1, $2);
        }
    |   type IDENTIFIER ASSIGN expr SEMICOLON
        {
            printf("   [decl]   tipo=%s nome=%s valor_inicial=%d\n", $1, $2, $4);
        }
    |   type IDENTIFIER ASSIGN STRING_LITERAL SEMICOLON
        {
            printf("   [decl]   tipo=%s nome=%s valor_inicial=\"%s\"\n", $1, $2, $4);
        }
    ;

assign_op:
        ASSIGN         { $$ = "="; }
    |   PLUS_ASSIGN    { $$ = "+="; }
    |   MINUS_ASSIGN   { $$ = "-="; }
    |   MULT_ASSIGN    { $$ = "*="; }
    |   DIV_ASSIGN     { $$ = "/="; }
    ;

assign_stmt:
        IDENTIFIER assign_op expr SEMICOLON
        {
            printf("   [attrib] %s %s %d\n", $1, $2, $3);
        }
    ;

increment_stmt:
        IDENTIFIER INCREMENT SEMICOLON
        {
            printf("   [incr]   %s++\n", $1);
        }
    |   IDENTIFIER DECREMENT SEMICOLON
        {
            printf("   [decr]   %s--\n", $1);
        }
    |   INCREMENT IDENTIFIER SEMICOLON
        {
            printf("   [incr]   ++%s\n", $2);
        }
    |   DECREMENT IDENTIFIER SEMICOLON
        {
            printf("   [decr]   --%s\n", $2);
        }
    ;

/*
 * Construcao simplificada equivalente a System.out.println(...).
 * O ponto (.) e reconhecido como token separado pelo lexer; e o parser
 * quem reconhece a sequencia System . out . println ( ... ) ;
 */
print_stmt:
        IDENTIFIER DOT IDENTIFIER DOT IDENTIFIER LPAREN print_arg RPAREN SEMICOLON
        {
            if (strcmp($1, "System") != 0 || strcmp($3, "out") != 0 || strcmp($5, "println") != 0) {
                yyerror("construcao de impressao invalida (esperado System.out.println)");
                YYERROR;
            }
            printf("   [print]  System.out.println(...)\n");
        }
    ;

print_arg:
        expr
    |   STRING_LITERAL
        {
            printf("   [print-arg] string=\"%s\"\n", $1);
        }
    ;

if_stmt:
        IF LPAREN expr RPAREN block
        {
            printf("   [if]     condicao avaliada\n");
        }
    |   IF LPAREN expr RPAREN block ELSE block
        {
            printf("   [if/else] condicao avaliada\n");
        }
    |   IF LPAREN expr RPAREN block ELSE if_stmt
        {
            printf("   [if/else if] condicao avaliada\n");
        }
    ;

while_stmt:
        WHILE LPAREN expr RPAREN block
        {
            printf("   [while]  condicao avaliada\n");
        }
    ;

/*
 * for ( init ; condicao ; atualizacao ) bloco
 * Os tres campos sao opcionais, como na spec ([<init>] ; [<cond>] ; [<inc>]).
 */
for_stmt:
        FOR LPAREN for_init SEMICOLON for_cond SEMICOLON for_update RPAREN block
        {
            printf("   [for]    laco reconhecido\n");
        }
    ;

for_init:
        /* vazio */
    |   type IDENTIFIER ASSIGN expr
        {
            printf("   [for-init] tipo=%s nome=%s valor_inicial=%d\n", $1, $2, $4);
        }
    |   IDENTIFIER assign_op expr
    ;

for_cond:
        /* vazio */
    |   expr
    ;

for_update:
        /* vazio */
    |   IDENTIFIER assign_op expr
    |   IDENTIFIER INCREMENT
    |   IDENTIFIER DECREMENT
    |   INCREMENT IDENTIFIER
    |   DECREMENT IDENTIFIER
    ;

do_while_stmt:
        DO block WHILE LPAREN expr RPAREN SEMICOLON
        {
            printf("   [do-while] laco reconhecido\n");
        }
    ;

break_stmt:
        BREAK SEMICOLON
        {
            printf("   [break]\n");
        }
    ;

continue_stmt:
        CONTINUE SEMICOLON
        {
            printf("   [continue]\n");
        }
    ;

return_stmt:
        RETURN SEMICOLON
        {
            printf("   [return] sem valor\n");
        }
    |   RETURN expr SEMICOLON
        {
            printf("   [return] com valor\n");
        }
    ;

expr:
        expr PLUS expr           { $$ = $1 + $3; }
    |   expr MINUS expr          { $$ = $1 - $3; }
    |   expr MULTIPLY expr       { $$ = $1 * $3; }
    |   expr DIVIDE expr         { $$ = $3 != 0 ? $1 / $3 : 0; }
    |   expr MODULO expr         { $$ = $3 != 0 ? $1 % $3 : 0; }
    |   expr LESS expr           { $$ = $1 < $3; }
    |   expr GREATER expr        { $$ = $1 > $3; }
    |   expr LESS_EQUAL expr     { $$ = $1 <= $3; }
    |   expr GREATER_EQUAL expr  { $$ = $1 >= $3; }
    |   expr EQUAL expr          { $$ = $1 == $3; }
    |   expr NOT_EQUAL expr      { $$ = $1 != $3; }
    |   expr AND expr            { $$ = $1 && $3; }
    |   expr OR expr             { $$ = $1 || $3; }
    |   NOT expr                 { $$ = !$2; }
    |   MINUS expr %prec UMINUS  { $$ = -$2; }
    |   LPAREN expr RPAREN       { $$ = $2; }
    |   IDENTIFIER               { $$ = 0; /* prototipo: sem tabela de simbolos */ }
    |   INTEGER_LITERAL          { $$ = $1; }
    |   REAL_LITERAL             { $$ = (int) $1; }
    |   CHAR_LITERAL             { $$ = $1[0]; }
    |   TRUE                     { $$ = 1; }
    |   FALSE                    { $$ = 0; }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "ERRO DE SINTAXE na linha %d: %s\n", yylineno, s);
}


