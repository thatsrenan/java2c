/*
 * parser.y
 *
 * Analisador sintatico (Bison) do compilador Java -> C. Segue a gramatica
 * de docs/definicao-escopo-sintaxe-semantica.md:
 *
 *   - classe:   [public] class Nome { membros }
 *   - membros:  campos static (static <tipo> x [= expr];) e
 *               metodos static ([public] static <tipo>|void nome(params) bloco)
 *   - main:     o parametro especial "String[] args" so e aceito em main
 *   - declaracao de variavel local (int, float, double, boolean, char)
 *   - atribuicao (=, +=, -=, *=, /=)
 *   - expressoes aritmeticas, relacionais e logicas com precedencia,
 *     chamadas de metodo e cast explicito ((int) x)
 *   - if / else if / else
 *   - while, for, do-while, break, continue, return
 *   - incremento/decremento (i++, ++i, i--, --i)
 *   - blocos { ... }
 *   - construcao simplificada System.out.println(...)
 *
 * "String" so aparece em "String[] args" e literais de string so dentro do
 * println (spec 1.4); qualquer outro uso e rejeitado com mensagem propria.
 *
 * Alem da forma com classe, o programa pode vir "solto": uma sequencia de
 * campos static, metodos e statements sem a classe em volta. E a forma
 * usada pelos casos de examples/escopo e por examples/exemplo1.java.
 *
 * As acoes montam a AST (parser/ast.h) e, ao final de uma analise sem
 * erros, a raiz fica em ast_root. A linha de cada no vem da posicao do
 * token que o identifica (%locations; o lexer preenche yylloc).
 */

%code requires {
#include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yylex(void);
void yyerror(const char *s);
extern int yylineno;
%}

/* %code (sem qualificador) sai depois do %code requires, entao ja enxerga Node */
%code {
/* 1 quando o ultimo param_opt reduzido foi o "String[] args" do main.
 * Seguro como global: metodos nao se aninham, entao nenhum outro
 * param_opt e reduzido entre os parametros e o fim do proprio metodo. */
static int params_were_main_args = 0;

/* Bloco que acumula os statements soltos de um programa sem classe. */
static Node *loose_block = NULL;
static Node *add_top_item(Node *members, Node *item);
}

%locations

%union {
    int     ival;
    double  dval;
    char   *sval;
    Node   *node;
    Type    tval;
    OpKind  opval;
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

%type <tval>  type
%type <opval> assign_op
%type <node>  class_decl member_list member top_list top_item
%type <node>  field_decl method_decl method_sig param_opt param_list param
%type <node>  stmt_list stmt block vardecl assign_stmt increment_stmt
%type <node>  call_stmt call arg_opt arg_list print_stmt print_arg
%type <node>  if_stmt while_stmt for_stmt for_init for_cond for_update
%type <node>  do_while_stmt break_stmt continue_stmt return_stmt expr

/* Em erro de sintaxe, o Bison descarta o que estava na pilha: sem isto,
 * as subarvores e strings ja montadas vazariam. ('program' nao tem tipo
 * de proposito: o Bison chamaria o destrutor na raiz ao terminar.) */
%destructor { free_ast($$); } <node>
%destructor { free($$); } <sval>

/* Mensagens de erro com o token encontrado e os esperados */
%define parse.error verbose

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
        class_decl        { ast_root = $1; }
    |   top_list          { ast_root = new_class(NULL, $1, 1); }
    |   /* vazio */       { ast_root = new_class(NULL, NULL, 1); }
    ;

/* ================= Estrutura do programa ================= */

/*
 * PUBLIC aparece explicito nas alternativas (em vez de um "opt_public"
 * vazio) para nao gerar conflito: num programa solto, ao ver STATIC o
 * parser nao saberia se reduz o opcional vazio (metodo) ou nao (campo).
 */
class_decl:
        CLASS IDENTIFIER LBRACE member_list RBRACE
        { $$ = new_class($2, $4, @1.first_line); }
    |   PUBLIC CLASS IDENTIFIER LBRACE member_list RBRACE
        { $$ = new_class($3, $5, @2.first_line); }
    ;

member_list:
        /* vazio */               { $$ = NULL; }
    |   member_list member        { $$ = append($1, $2); }
    ;

member:
        field_decl
    |   method_decl
    ;

/* programa sem a classe em volta: membros e statements em qualquer ordem */
top_list:
        top_item                  { loose_block = NULL; $$ = add_top_item(NULL, $1); }
    |   top_list top_item         { $$ = add_top_item($1, $2); }
    ;

top_item:
        field_decl
    |   method_decl
    |   stmt
    ;

field_decl:
        STATIC type IDENTIFIER SEMICOLON
        { $$ = new_field($2, $3, NULL, @3.first_line); }
    |   STATIC type IDENTIFIER ASSIGN expr SEMICOLON
        { $$ = new_field($2, $3, $5, @3.first_line); }
    ;

method_decl:
        STATIC method_sig             { $$ = $2; }
    |   PUBLIC STATIC method_sig      { $$ = $3; }
    ;

/*
 * O tipo de retorno fica dentro de method_sig (e nao num "ret_type") para
 * que "static int x" so precise decidir entre campo e metodo ao ver o
 * token depois do nome: ';'/'=' (campo) ou '(' (metodo).
 */
method_sig:
        type IDENTIFIER LPAREN param_opt RPAREN block
        {
            if (params_were_main_args && strcmp($2, "main") != 0) {
                yyerror("\"String[] args\" so e permitido como parametro de main");
                /* YYERROR nao libera os simbolos desta regra */
                free($2); free_ast($4); free_ast($6);
                YYERROR;
            }
            $$ = new_method($1, $2, $4, $6, @2.first_line);
        }
    |   VOID IDENTIFIER LPAREN param_opt RPAREN block
        {
            if (params_were_main_args && strcmp($2, "main") != 0) {
                yyerror("\"String[] args\" so e permitido como parametro de main");
                free($2); free_ast($4); free_ast($6);
                YYERROR;
            }
            $$ = new_method(TYPE_VOID, $2, $4, $6, @2.first_line);
        }
    ;

/* "String[] args" nao vira parametro (ast.h): main fica com a = NULL */
param_opt:
        /* vazio */
        { $$ = NULL; params_were_main_args = 0; }
    |   param_list
        { $$ = $1; params_were_main_args = 0; }
    |   STRING_TYPE LBRACKET RBRACKET IDENTIFIER
        { $$ = NULL; params_were_main_args = 1; free($4); }
    ;

param_list:
        param                         { $$ = $1; }
    |   param_list COMMA param        { $$ = append($1, $3); }
    ;

param:
        type IDENTIFIER               { $$ = new_param($1, $2, @2.first_line); }
    ;

/* ================= Statements ================= */

stmt_list:
        /* vazio */                   { $$ = NULL; }
    |   stmt_list stmt                { $$ = append($1, $2); }
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
    |   call_stmt
    |   block
    ;

block:
        LBRACE stmt_list RBRACE
        {
            $$ = new_node(NODE_BLOCK, @1.first_line);
            $$->a = $2;
        }
    ;

type:
        INT          { $$ = TYPE_INT; }
    |   FLOAT        { $$ = TYPE_FLOAT; }
    |   DOUBLE       { $$ = TYPE_DOUBLE; }
    |   BOOLEAN      { $$ = TYPE_BOOLEAN; }
    |   CHAR         { $$ = TYPE_CHAR; }
        /* String nao e tipo do subconjunto (spec 1.4 / 13.2): reconhecido
         * aqui so para dar uma mensagem clara em vez de "syntax error" */
    |   STRING_TYPE
        {
            yyerror("o tipo String nao e suportado pelo subconjunto Java");
            YYERROR;
        }
    ;

vardecl:
        type IDENTIFIER SEMICOLON
        { $$ = new_vardecl($1, $2, NULL, @2.first_line); }
    |   type IDENTIFIER ASSIGN expr SEMICOLON
        { $$ = new_vardecl($1, $2, $4, @2.first_line); }
    ;

assign_op:
        ASSIGN         { $$ = OP_ASSIGN; }
    |   PLUS_ASSIGN    { $$ = OP_ADD_ASSIGN; }
    |   MINUS_ASSIGN   { $$ = OP_SUB_ASSIGN; }
    |   MULT_ASSIGN    { $$ = OP_MULT_ASSIGN; }
    |   DIV_ASSIGN     { $$ = OP_DIV_ASSIGN; }
    ;

assign_stmt:
        IDENTIFIER assign_op expr SEMICOLON
        { $$ = new_assign($2, $1, $3, @1.first_line); }
    ;

increment_stmt:
        IDENTIFIER INCREMENT SEMICOLON    { $$ = new_incdec(OP_POSTINC, $1, @1.first_line); }
    |   IDENTIFIER DECREMENT SEMICOLON    { $$ = new_incdec(OP_POSTDEC, $1, @1.first_line); }
    |   INCREMENT IDENTIFIER SEMICOLON    { $$ = new_incdec(OP_PREINC, $2, @2.first_line); }
    |   DECREMENT IDENTIFIER SEMICOLON    { $$ = new_incdec(OP_PREDEC, $2, @2.first_line); }
    ;

/* chamada de metodo como statement: f(a, b); */
call_stmt:
        call SEMICOLON                    { $$ = $1; }
    ;

call:
        IDENTIFIER LPAREN arg_opt RPAREN
        { $$ = new_call($1, $3, @1.first_line); }
    ;

arg_opt:
        /* vazio */                       { $$ = NULL; }
    |   arg_list                          { $$ = $1; }
    ;

arg_list:
        expr                              { $$ = $1; }
    |   arg_list COMMA expr               { $$ = append($1, $3); }
    ;

/*
 * Construcao simplificada equivalente a System.out.println(...).
 * O ponto (.) e reconhecido como token separado pelo lexer; e o parser
 * quem reconhece a sequencia System . out . println ( ... ) ;
 */
print_stmt:
        IDENTIFIER DOT IDENTIFIER DOT IDENTIFIER LPAREN print_arg RPAREN SEMICOLON
        {
            int ok = strcmp($1, "System") == 0 && strcmp($3, "out") == 0
                  && strcmp($5, "println") == 0;
            free($1); free($3); free($5);
            if (!ok) {
                yyerror("construcao de impressao invalida (esperado System.out.println)");
                free_ast($7);
                YYERROR;
            }
            $$ = new_node(NODE_PRINT, @1.first_line);
            $$->a = $7;
        }
    ;

print_arg:
        expr                              { $$ = $1; }
    |   STRING_LITERAL                    { $$ = new_str($1, @1.first_line); }
    ;

if_stmt:
        IF LPAREN expr RPAREN block
        {
            $$ = new_node(NODE_IF, @1.first_line);
            $$->a = $3; $$->b = $5;
        }
    |   IF LPAREN expr RPAREN block ELSE block
        {
            $$ = new_node(NODE_IF, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = $7;
        }
    |   IF LPAREN expr RPAREN block ELSE if_stmt
        {
            $$ = new_node(NODE_IF, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = $7;
        }
    ;

while_stmt:
        WHILE LPAREN expr RPAREN block
        {
            $$ = new_node(NODE_WHILE, @1.first_line);
            $$->a = $3; $$->b = $5;
        }
    ;

/*
 * for ( init ; condicao ; atualizacao ) bloco
 * Os tres campos sao opcionais, como na spec ([<init>] ; [<cond>] ; [<inc>]).
 */
for_stmt:
        FOR LPAREN for_init SEMICOLON for_cond SEMICOLON for_update RPAREN block
        {
            $$ = new_node(NODE_FOR, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = $7; $$->d = $9;
        }
    ;

for_init:
        /* vazio */                       { $$ = NULL; }
    |   type IDENTIFIER ASSIGN expr       { $$ = new_vardecl($1, $2, $4, @2.first_line); }
    |   IDENTIFIER assign_op expr         { $$ = new_assign($2, $1, $3, @1.first_line); }
    ;

for_cond:
        /* vazio */                       { $$ = NULL; }
    |   expr                              { $$ = $1; }
    ;

for_update:
        /* vazio */                       { $$ = NULL; }
    |   IDENTIFIER assign_op expr         { $$ = new_assign($2, $1, $3, @1.first_line); }
    |   IDENTIFIER INCREMENT              { $$ = new_incdec(OP_POSTINC, $1, @1.first_line); }
    |   IDENTIFIER DECREMENT              { $$ = new_incdec(OP_POSTDEC, $1, @1.first_line); }
    |   INCREMENT IDENTIFIER              { $$ = new_incdec(OP_PREINC, $2, @2.first_line); }
    |   DECREMENT IDENTIFIER              { $$ = new_incdec(OP_PREDEC, $2, @2.first_line); }
    ;

do_while_stmt:
        DO block WHILE LPAREN expr RPAREN SEMICOLON
        {
            $$ = new_node(NODE_DOWHILE, @1.first_line);
            $$->a = $2; $$->b = $5;
        }
    ;

break_stmt:
        BREAK SEMICOLON                   { $$ = new_node(NODE_BREAK, @1.first_line); }
    ;

continue_stmt:
        CONTINUE SEMICOLON                { $$ = new_node(NODE_CONTINUE, @1.first_line); }
    ;

return_stmt:
        RETURN SEMICOLON                  { $$ = new_node(NODE_RETURN, @1.first_line); }
    |   RETURN expr SEMICOLON
        {
            $$ = new_node(NODE_RETURN, @1.first_line);
            $$->a = $2;
        }
    ;

/* A linha de um operador binario e a do proprio operador (@2). */
expr:
        expr PLUS expr           { $$ = new_binop(OP_ADD,  $1, $3, @2.first_line); }
    |   expr MINUS expr          { $$ = new_binop(OP_SUB,  $1, $3, @2.first_line); }
    |   expr MULTIPLY expr       { $$ = new_binop(OP_MULT, $1, $3, @2.first_line); }
    |   expr DIVIDE expr         { $$ = new_binop(OP_DIV,  $1, $3, @2.first_line); }
    |   expr MODULO expr         { $$ = new_binop(OP_MOD,  $1, $3, @2.first_line); }
    |   expr LESS expr           { $$ = new_binop(OP_LT,   $1, $3, @2.first_line); }
    |   expr GREATER expr        { $$ = new_binop(OP_GT,   $1, $3, @2.first_line); }
    |   expr LESS_EQUAL expr     { $$ = new_binop(OP_LE,   $1, $3, @2.first_line); }
    |   expr GREATER_EQUAL expr  { $$ = new_binop(OP_GE,   $1, $3, @2.first_line); }
    |   expr EQUAL expr          { $$ = new_binop(OP_EQ,   $1, $3, @2.first_line); }
    |   expr NOT_EQUAL expr      { $$ = new_binop(OP_NE,   $1, $3, @2.first_line); }
    |   expr AND expr            { $$ = new_binop(OP_AND,  $1, $3, @2.first_line); }
    |   expr OR expr             { $$ = new_binop(OP_OR,   $1, $3, @2.first_line); }
    |   NOT expr                 { $$ = new_unary(OP_NOT, $2, @1.first_line); }
    |   MINUS expr %prec UMINUS  { $$ = new_unary(OP_NEG, $2, @1.first_line); }
    |   LPAREN expr RPAREN       { $$ = $2; }
    |   LPAREN type RPAREN expr %prec UMINUS
                                 { $$ = new_cast($2, $4, @1.first_line); }
    |   call                     { $$ = $1; }
    |   IDENTIFIER               { $$ = new_ident($1, @1.first_line); }
    |   INTEGER_LITERAL          { $$ = new_int($1, @1.first_line); }
    |   REAL_LITERAL             { $$ = new_real($1, @1.first_line); }
    |   CHAR_LITERAL             { $$ = new_char($1, @1.first_line); }
    |   TRUE                     { $$ = new_bool(1, @1.first_line); }
    |   FALSE                    { $$ = new_bool(0, @1.first_line); }
    ;

%%

/*
 * Programa solto: campos e metodos viram membros da classe implicita;
 * statements vao, na ordem, para um unico NODE_BLOCK membro, criado onde
 * aparece o primeiro statement (ver NODE_CLASS em ast.h).
 */
static Node *add_top_item(Node *members, Node *item) {
    int is_member = item->kind == NODE_METHOD
                 || (item->kind == NODE_VARDECL && item->is_global);
    if (is_member)
        return append(members, item);

    if (!loose_block) {
        loose_block = new_node(NODE_BLOCK, item->line);
        members = append(members, loose_block);
    }
    loose_block->a = append(loose_block->a, item);
    return members;
}

void yyerror(const char *s) {
    fprintf(stderr, "ERRO DE SINTAXE na linha %d: %s\n", yylineno, s);
}
