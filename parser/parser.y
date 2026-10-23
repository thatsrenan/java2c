/*
 * parser.y
 *
 * Analisador sintatico (Bison) do compilador Java -> C, com analise de
 * escopo/declaracao usando a tabela de simbolos em src/symbol_table.c.
 *
 * Cobre o subconjunto procedural descrito em
 * docs/definicao-escopo-sintaxe-semantica.md:
 *
 *   - campos static (globais em C) e metodos static;
 *   - declaracao de variavel (int, float, double, boolean, char);
 *   - atribuicao (=, +=, -=, *=, /=);
 *   - expressoes aritmeticas, relacionais, logicas e chamadas de funcao;
 *   - if / else if / else;
 *   - while, for, do-while, break, continue, return;
 *   - incremento/decremento (i++, ++i, i--, --i);
 *   - blocos { ... } com escopo proprio;
 *   - construcao simplificada System.out.println(...).
 *
 * Erros semanticos sao reportados como "ERRO SEMANTICO na linha N: ..." e
 * marcam had_semantic_error, que main.c consulta para decidir o status final.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "symbol_table.h"

int yylex(void);
void yyerror(const char *s);
extern int yylineno;

/* main.c consulta esta flag para nao declarar sucesso quando houve erro. */
int had_semantic_error = 0;

/* main.c ajusta esta flag quando o usuario passa --tabela. */
int show_symbol_table = 0;

/* Contexto do metodo sendo analisado (nao ha metodos aninhados). */
static Symbol   *current_function = NULL;
static DataType  current_return_type = TYPE_VOID;

/* Profundidade de lacos, para validar break/continue. */
static int loop_depth = 0;

static void semantic_error(const char *fmt, ...) {
    va_list ap;

    fprintf(stderr, "ERRO SEMANTICO na linha %d: ", yylineno);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    had_semantic_error = 1;
}

/* ------------------------------------------------------------------ */
/* Verificacoes de tipo                                               */
/* ------------------------------------------------------------------ */

static int is_numeric(DataType type) {
    return type == TYPE_INT || type == TYPE_FLOAT ||
           type == TYPE_DOUBLE || type == TYPE_CHAR;
}

static int is_integral(DataType type) {
    return type == TYPE_INT || type == TYPE_CHAR;
}

/* Conversoes implicitas permitidas no subconjunto (promocao). */
static int is_assignable(DataType dst, DataType src) {
    if (dst == TYPE_UNKNOWN || src == TYPE_UNKNOWN) return 1;
    if (dst == src) return 1;

    if (dst == TYPE_DOUBLE &&
        (src == TYPE_FLOAT || src == TYPE_INT || src == TYPE_CHAR)) return 1;
    if (dst == TYPE_FLOAT && (src == TYPE_INT || src == TYPE_CHAR)) return 1;
    if (dst == TYPE_INT && src == TYPE_CHAR) return 1;

    return 0;
}

static DataType promote(DataType a, DataType b) {
    if (a == TYPE_UNKNOWN || b == TYPE_UNKNOWN) return TYPE_UNKNOWN;
    if (a == TYPE_DOUBLE || b == TYPE_DOUBLE) return TYPE_DOUBLE;
    if (a == TYPE_FLOAT || b == TYPE_FLOAT) return TYPE_FLOAT;
    return TYPE_INT; /* int/char em aritmetica resultam em int */
}

static void require_numeric(DataType type, const char *context) {
    if (type != TYPE_UNKNOWN && !is_numeric(type)) {
        semantic_error("%s exige tipo numerico (tipo encontrado: %s)",
                       context, st_type_name(type));
    }
}

static void require_integral(DataType type, const char *context) {
    if (type != TYPE_UNKNOWN && !is_integral(type)) {
        semantic_error("%s exige tipo inteiro (tipo encontrado: %s)",
                       context, st_type_name(type));
    }
}

static void require_boolean(DataType type, const char *context) {
    if (type != TYPE_UNKNOWN && type != TYPE_BOOLEAN) {
        semantic_error("%s deve possuir tipo boolean (tipo encontrado: %s)",
                       context, st_type_name(type));
    }
}

static void check_comparable(DataType a, DataType b, const char *context) {
    if (a == TYPE_UNKNOWN || b == TYPE_UNKNOWN) return;
    if (a == b) return;
    if (is_numeric(a) && is_numeric(b)) return;
    semantic_error("tipos incompativeis na comparacao em %s (%s e %s)",
                   context, st_type_name(a), st_type_name(b));
}

static void check_arithmetic(DataType a, DataType b, const char *op) {
    char context[64];
    snprintf(context, sizeof context, "o operador '%s'", op);
    require_numeric(a, context);
    require_numeric(b, context);
}

static void check_assignment(Symbol *var, const char *op, DataType rhs) {
    if (!var) return;

    if (strcmp(op, "=") == 0) {
        if (!is_assignable(var->type, rhs)) {
            semantic_error("tipos incompativeis: nao e possivel atribuir %s a '%s' (%s)",
                           st_type_name(rhs), var->name, st_type_name(var->type));
        }
        return;
    }

    if (var->type != TYPE_UNKNOWN && !is_numeric(var->type)) {
        semantic_error("o operador '%s' exige uma variavel numerica ('%s' e %s)",
                       op, var->name, st_type_name(var->type));
    }
    if (rhs != TYPE_UNKNOWN && !is_numeric(rhs)) {
        semantic_error("o operador '%s' exige um valor numerico (recebeu %s)",
                       op, st_type_name(rhs));
    }
}

static void check_increment(Symbol *var, const char *op) {
    if (!var) return;
    if (var->type != TYPE_UNKNOWN && !is_numeric(var->type)) {
        semantic_error("o operador '%s' exige uma variavel numerica ('%s' e %s)",
                       op, var->name, st_type_name(var->type));
    }
}

/* ------------------------------------------------------------------ */
/* Declaracao e busca de simbolos                                     */
/* ------------------------------------------------------------------ */

static Symbol *declare_symbol(const char *name, DataType type,
                              SymbolKind kind, int line) {
    Symbol *symbol = NULL;

    switch (kind) {
    case SYM_VARIABLE:
        /* Uma variavel local nao pode ocultar outra variavel local visivel
         * (bloco externo, parametro ou variavel de for). Ocultar uma global
         * e permitido. */
        if (st_lookup_local_visible(name) != NULL) {
            semantic_error("a variavel '%s' ja foi declarada neste escopo", name);
            return NULL;
        }
        symbol = st_insert(name, type, kind, line);
        break;

    case SYM_PARAMETER:
        symbol = st_insert(name, type, kind, line);
        if (!symbol) semantic_error("o parametro '%s' ja foi declarado", name);
        break;

    case SYM_GLOBAL:
        symbol = st_insert_global(name, type, kind, line);
        if (!symbol) semantic_error("'%s' ja foi declarado no escopo global", name);
        break;

    case SYM_FUNCTION:
        symbol = st_insert_global(name, type, kind, line);
        if (!symbol) {
            semantic_error("a funcao '%s' ja foi declarada (sobrecarga nao e suportada)", name);
        }
        break;
    }

    if (!symbol && kind == SYM_VARIABLE) {
        /* Corrida improvavel: nome passou pela checagem mas ja existia. */
        semantic_error("a variavel '%s' ja foi declarada neste escopo", name);
    }

    return symbol;
}

static Symbol *declare_variable(const char *name, DataType type, int line,
                                int initialized) {
    Symbol *symbol;

    if (type == TYPE_VOID) {
        semantic_error("a variavel '%s' nao pode ter tipo void", name);
    } else if (type == TYPE_STRING) {
        semantic_error("o tipo String nao e suportado pelo subconjunto Java");
    }

    SymbolKind kind = st_is_global_scope() ? SYM_GLOBAL : SYM_VARIABLE;
    symbol = declare_symbol(name, type, kind, line);
    if (symbol && initialized) st_mark_initialized(symbol);
    return symbol;
}

static Symbol *declare_global(const char *name, DataType type, int line,
                              int initialized) {
    Symbol *symbol;

    if (type == TYPE_VOID) {
        semantic_error("o campo '%s' nao pode ter tipo void", name);
    } else if (type == TYPE_STRING) {
        semantic_error("o tipo String nao e suportado pelo subconjunto Java");
    }

    symbol = declare_symbol(name, type, SYM_GLOBAL, line);
    if (symbol && initialized) st_mark_initialized(symbol);
    return symbol;
}

static Symbol *declare_function(const char *name, DataType return_type, int line) {
    Symbol *symbol;

    if (return_type == TYPE_STRING) {
        semantic_error("o tipo String nao e suportado como retorno de metodo");
    }

    symbol = declare_symbol(name, return_type, SYM_FUNCTION, line);
    if (symbol) st_mark_initialized(symbol);
    return symbol;
}

static Symbol *declare_parameter(const char *name, DataType type, int line,
                                 int allow_string_array) {
    Symbol *symbol;

    if (type == TYPE_VOID) {
        semantic_error("o parametro '%s' nao pode ter tipo void", name);
    } else if (type == TYPE_STRING && !allow_string_array) {
        semantic_error("o tipo String nao e suportado como parametro");
    }

    symbol = declare_symbol(name, type, SYM_PARAMETER, line);
    if (symbol) {
        st_mark_initialized(symbol);
        if (current_function) st_add_parameter(current_function, type);
    }
    return symbol;
}

static Symbol *lookup_variable(const char *name) {
    Symbol *symbol = st_lookup(name);

    if (!symbol) {
        semantic_error("a variavel '%s' nao foi declarada", name);
        return NULL;
    }
    if (symbol->kind == SYM_FUNCTION) {
        semantic_error("'%s' e uma funcao e nao pode ser usada como variavel", name);
        return NULL;
    }
    return symbol;
}

static Symbol *lookup_function(const char *name, int argc) {
    Symbol *symbol = st_lookup_global(name);

    if (!symbol) {
        semantic_error("a funcao '%s' nao foi declarada antes do uso", name);
        return NULL;
    }
    if (symbol->kind != SYM_FUNCTION) {
        semantic_error("'%s' nao e uma funcao", name);
        return NULL;
    }
    if (symbol->param_count != argc) {
        semantic_error("a funcao '%s' espera %d argumento(s), mas recebeu %d",
                       name, symbol->param_count, argc);
    }
    return symbol;
}
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

/* O valor de 'type' e um DataType; o de 'expr' e o tipo estatico resultante. */
%type <ival> type expr args_opt arg_list global_init
%type <sval> assign_op

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
        /* Escopo "main" implicito: comandos soltos do topo do arquivo se
         * comportam como locais de um metodo, enquanto campos static e
         * funcoes sao inseridos no escopo global. */
        { st_enter_scope(); current_return_type = TYPE_VOID; }
        top_list
        {
            if (show_symbol_table) st_print(stdout);
            st_exit_scope();
            if (!had_semantic_error) {
                printf(">> Programa reconhecido com sucesso.\n");
            }
        }
    ;

top_list:
        /* vazio */
    |   top_list top_decl
    ;

top_decl:
        method_decl
    |   global_field
    |   class_decl
    |   stmt
    ;

/* ------------------------------------------------------------------ */
/* Classes (apenas a casca sintatica; membros sao globais/funcoes)    */
/* ------------------------------------------------------------------ */

class_decl:
        opt_public CLASS IDENTIFIER LBRACE class_body RBRACE
    ;

class_body:
        /* vazio */
    |   class_body class_member
    ;

class_member:
        method_decl
    |   global_field
    |   stmt
    ;

opt_public:
        /* vazio */
    |   PUBLIC
    ;

/* ------------------------------------------------------------------ */
/* Metodos static                                                     */
/* ------------------------------------------------------------------ */

method_decl:
        opt_public STATIC type IDENTIFIER
        {
            Symbol *function = declare_function($4, $3, yylineno);
            current_function = function;
            current_return_type = $3;
            st_enter_function_scope();
            printf("   [metodo] tipo_retorno=%s nome=%s\n",
                   st_type_name($3), $4);
        }
        LPAREN param_list_opt RPAREN block
        {
            st_exit_scope();
            current_function = NULL;
            current_return_type = TYPE_VOID;
        }
    ;

param_list_opt:
        /* vazio */
    |   param_list
    ;

param_list:
        param
    |   param_list COMMA param
    ;

param:
        type IDENTIFIER
        {
            declare_parameter($2, $1, yylineno, 0);
        }
    |   /* unica forma de array aceita: String[] args do main */
        STRING_TYPE LBRACKET RBRACKET IDENTIFIER
        {
            if (!current_function || strcmp(current_function->name, "main") != 0) {
                semantic_error("arrays e o tipo String so sao suportados no parametro String[] args do main");
            }
            declare_parameter($4, TYPE_STRING, yylineno, 1);
        }
    ;

/* Campo static: vira variavel global em C. */
global_field:
        opt_public STATIC type IDENTIFIER global_init SEMICOLON
        {
            Symbol *symbol = declare_global($4, $3, yylineno, $5 >= 0);
            if (symbol && $5 >= 0 && !is_assignable(symbol->type, (DataType) $5)) {
                semantic_error("tipos incompativeis no inicializador de '%s' (%s <- %s)",
                               symbol->name, st_type_name(symbol->type),
                               st_type_name((DataType) $5));
            }
            printf("   [global] tipo=%s nome=%s\n", st_type_name($3), $4);
        }
    ;

global_init:
        /* vazio */
        { $$ = -1; }
    |   ASSIGN expr
        { $$ = (int) $2; }
    ;

/* ------------------------------------------------------------------ */
/* Comandos                                                           */
/* ------------------------------------------------------------------ */

stmt_list:
        /* vazio */
    |   stmt_list stmt
    ;

stmt:
        vardecl
    |   assign_stmt
    |   call_stmt
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
        LBRACE
        { st_enter_scope(); }
        stmt_list
        RBRACE
        { st_exit_scope(); }
    ;

type:
        INT          { $$ = TYPE_INT; }
    |   FLOAT        { $$ = TYPE_FLOAT; }
    |   DOUBLE       { $$ = TYPE_DOUBLE; }
    |   BOOLEAN      { $$ = TYPE_BOOLEAN; }
    |   CHAR         { $$ = TYPE_CHAR; }
    |   VOID         { $$ = TYPE_VOID; }
    |   STRING_TYPE  { $$ = TYPE_STRING; }
    ;

vardecl:
        type IDENTIFIER SEMICOLON
        {
            Symbol *symbol = declare_variable($2, $1, yylineno, 0);
            if (symbol) {
                printf("   [decl]   tipo=%s nome=%s\n",
                       st_type_name(symbol->type), symbol->name);
            }
        }
    |   type IDENTIFIER ASSIGN expr SEMICOLON
        {
            Symbol *symbol = declare_variable($2, $1, yylineno, 1);
            if (symbol) {
                if (!is_assignable(symbol->type, $4)) {
                    semantic_error("tipos incompativeis no inicializador de '%s' (%s <- %s)",
                                   symbol->name, st_type_name(symbol->type),
                                   st_type_name($4));
                }
                printf("   [decl]   tipo=%s nome=%s (inicializado)\n",
                       st_type_name(symbol->type), symbol->name);
            }
        }
    |   type IDENTIFIER ASSIGN STRING_LITERAL SEMICOLON
        {
            if ($1 != TYPE_STRING) {
                semantic_error("literal de string so pode ser usado em System.out.println");
            }
            Symbol *symbol = declare_variable($2, $1, yylineno, 1);
            if (symbol) {
                printf("   [decl]   tipo=%s nome=%s (string)\n",
                       st_type_name(symbol->type), symbol->name);
            }
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
            Symbol *symbol = lookup_variable($1);
            check_assignment(symbol, $2, $3);
            if (symbol) {
                printf("   [attrib] %s %s (%s)\n", $1, $2, st_type_name($3));
            }
        }
    ;

call_stmt:
        IDENTIFIER LPAREN args_opt RPAREN SEMICOLON
        {
            lookup_function($1, $3);
            printf("   [chamada] %s(...)\n", $1);
        }
    ;

increment_stmt:
        IDENTIFIER INCREMENT SEMICOLON
        {
            Symbol *symbol = lookup_variable($1);
            check_increment(symbol, "++");
            printf("   [incr]   %s++\n", $1);
        }
    |   IDENTIFIER DECREMENT SEMICOLON
        {
            Symbol *symbol = lookup_variable($1);
            check_increment(symbol, "--");
            printf("   [decr]   %s--\n", $1);
        }
    |   INCREMENT IDENTIFIER SEMICOLON
        {
            Symbol *symbol = lookup_variable($2);
            check_increment(symbol, "++");
            printf("   [incr]   ++%s\n", $2);
        }
    |   DECREMENT IDENTIFIER SEMICOLON
        {
            Symbol *symbol = lookup_variable($2);
            check_increment(symbol, "--");
            printf("   [decr]   --%s\n", $2);
        }
    ;

/*
 * Construcao simplificada equivalente a System.out.println(...).
 * O ponto (.) e reconhecido como token separado pelo lexer; o parser
 * reconhece a sequencia System . out . println ( ... ) ;
 */
print_stmt:
        IDENTIFIER DOT IDENTIFIER DOT IDENTIFIER LPAREN print_arg RPAREN SEMICOLON
        {
            if (strcmp($1, "System") != 0 ||
                strcmp($3, "out") != 0 ||
                strcmp($5, "println") != 0) {
                semantic_error("construcao de impressao invalida (esperado System.out.println)");
            }
            printf("   [print]  System.out.println(...)\n");
        }
    ;

print_arg:
        expr
        {
            if ($1 == TYPE_VOID) {
                semantic_error("nao e possivel imprimir uma expressao de tipo void");
            }
        }
    |   STRING_LITERAL
        {
            printf("   [print-arg] string=\"%s\"\n", $1);
        }
    ;

if_stmt:
        IF LPAREN expr RPAREN block
        {
            require_boolean($3, "a condicao do if");
            printf("   [if]     condicao avaliada\n");
        }
    |   IF LPAREN expr RPAREN block ELSE block
        {
            require_boolean($3, "a condicao do if");
            printf("   [if/else] condicao avaliada\n");
        }
    |   IF LPAREN expr RPAREN block ELSE if_stmt
        {
            require_boolean($3, "a condicao do if");
            printf("   [if/else if] condicao avaliada\n");
        }
    ;

while_stmt:
        WHILE LPAREN expr RPAREN
        {
            require_boolean($3, "a condicao do while");
            loop_depth++;
        }
        block
        {
            loop_depth--;
            printf("   [while]  condicao avaliada\n");
        }
    ;

/*
 * for ( init ; condicao ; atualizacao ) bloco
 * Os tres campos sao opcionais, como na spec ([<init>] ; [<cond>] ; [<inc>]).
 * A variavel declarada no init so existe dentro do for.
 */
for_stmt:
        FOR LPAREN
        { st_enter_scope(); }
        for_init SEMICOLON for_cond SEMICOLON for_update RPAREN
        { loop_depth++; }
        block
        {
            loop_depth--;
            st_exit_scope();
            printf("   [for]    laco reconhecido\n");
        }
    ;

for_init:
        /* vazio */
    |   type IDENTIFIER ASSIGN expr
        {
            Symbol *symbol = declare_variable($2, $1, yylineno, 1);
            if (symbol && !is_assignable(symbol->type, $4)) {
                semantic_error("tipos incompativeis no inicializador de '%s' (%s <- %s)",
                               symbol->name, st_type_name(symbol->type),
                               st_type_name($4));
            }
            printf("   [for-init] tipo=%s nome=%s\n",
                   st_type_name($1), $2);
        }
    |   IDENTIFIER assign_op expr
        {
            Symbol *symbol = lookup_variable($1);
            check_assignment(symbol, $2, $3);
        }
    ;

for_cond:
        /* vazio */
    |   expr
        {
            require_boolean($1, "a condicao do for");
        }
    ;

for_update:
        /* vazio */
    |   IDENTIFIER assign_op expr
        {
            Symbol *symbol = lookup_variable($1);
            check_assignment(symbol, $2, $3);
        }
    |   IDENTIFIER INCREMENT
        {
            Symbol *symbol = lookup_variable($1);
            check_increment(symbol, "++");
        }
    |   IDENTIFIER DECREMENT
        {
            Symbol *symbol = lookup_variable($1);
            check_increment(symbol, "--");
        }
    |   INCREMENT IDENTIFIER
        {
            Symbol *symbol = lookup_variable($2);
            check_increment(symbol, "++");
        }
    |   DECREMENT IDENTIFIER
        {
            Symbol *symbol = lookup_variable($2);
            check_increment(symbol, "--");
        }
    ;

do_while_stmt:
        DO
        { loop_depth++; }
        block
        { loop_depth--; }
        WHILE LPAREN expr RPAREN SEMICOLON
        {
            require_boolean($7, "a condicao do do-while");
            printf("   [do-while] laco reconhecido\n");
        }
    ;

break_stmt:
        BREAK SEMICOLON
        {
            if (loop_depth == 0) {
                semantic_error("'break' so pode ser usado dentro de um laco");
            }
            printf("   [break]\n");
        }
    ;

continue_stmt:
        CONTINUE SEMICOLON
        {
            if (loop_depth == 0) {
                semantic_error("'continue' so pode ser usado dentro de um laco");
            }
            printf("   [continue]\n");
        }
    ;

return_stmt:
        RETURN SEMICOLON
        {
            if (current_return_type != TYPE_VOID) {
                semantic_error("o metodo deve retornar um valor do tipo %s",
                               st_type_name(current_return_type));
            }
            printf("   [return] sem valor\n");
        }
    |   RETURN expr SEMICOLON
        {
            if (current_return_type == TYPE_VOID) {
                semantic_error("metodo void nao pode retornar valor");
            } else if (!is_assignable(current_return_type, $2)) {
                semantic_error("tipo de retorno incompativel (%s esperado, %s encontrado)",
                               st_type_name(current_return_type), st_type_name($2));
            }
            printf("   [return] com valor (%s)\n", st_type_name($2));
        }
    ;

/* ------------------------------------------------------------------ */
/* Expressoes                                                         */
/* ------------------------------------------------------------------ */

args_opt:
        /* vazio */  { $$ = 0; }
    |   arg_list     { $$ = $1; }
    ;

arg_list:
        expr
        {
            if ($1 == TYPE_VOID) {
                semantic_error("nao e possivel passar uma expressao void como argumento");
            }
            $$ = 1;
        }
    |   arg_list COMMA expr
        {
            if ($3 == TYPE_VOID) {
                semantic_error("nao e possivel passar uma expressao void como argumento");
            }
            $$ = $1 + 1;
        }
    ;

expr:
        expr PLUS expr
        {
            check_arithmetic($1, $3, "+");
            $$ = promote($1, $3);
        }
    |   expr MINUS expr
        {
            check_arithmetic($1, $3, "-");
            $$ = promote($1, $3);
        }
    |   expr MULTIPLY expr
        {
            check_arithmetic($1, $3, "*");
            $$ = promote($1, $3);
        }
    |   expr DIVIDE expr
        {
            check_arithmetic($1, $3, "/");
            $$ = promote($1, $3);
        }
    |   expr MODULO expr
        {
            require_integral($1, "o operador '%'");
            require_integral($3, "o operador '%'");
            $$ = TYPE_INT;
        }
    |   expr LESS expr
        {
            char context[64];
            snprintf(context, sizeof context, "o operador '<'");
            require_numeric($1, context);
            require_numeric($3, context);
            $$ = TYPE_BOOLEAN;
        }
    |   expr GREATER expr
        {
            char context[64];
            snprintf(context, sizeof context, "o operador '>'");
            require_numeric($1, context);
            require_numeric($3, context);
            $$ = TYPE_BOOLEAN;
        }
    |   expr LESS_EQUAL expr
        {
            char context[64];
            snprintf(context, sizeof context, "o operador '<='");
            require_numeric($1, context);
            require_numeric($3, context);
            $$ = TYPE_BOOLEAN;
        }
    |   expr GREATER_EQUAL expr
        {
            char context[64];
            snprintf(context, sizeof context, "o operador '>='");
            require_numeric($1, context);
            require_numeric($3, context);
            $$ = TYPE_BOOLEAN;
        }
    |   expr EQUAL expr
        {
            check_comparable($1, $3, "o operador '=='");
            $$ = TYPE_BOOLEAN;
        }
    |   expr NOT_EQUAL expr
        {
            check_comparable($1, $3, "o operador '!='");
            $$ = TYPE_BOOLEAN;
        }
    |   expr AND expr
        {
            require_boolean($1, "o operador '&&'");
            require_boolean($3, "o operador '&&'");
            $$ = TYPE_BOOLEAN;
        }
    |   expr OR expr
        {
            require_boolean($1, "o operador '||'");
            require_boolean($3, "o operador '||'");
            $$ = TYPE_BOOLEAN;
        }
    |   NOT expr
        {
            require_boolean($2, "o operador '!'");
            $$ = TYPE_BOOLEAN;
        }
    |   MINUS expr %prec UMINUS
        {
            require_numeric($2, "o operador unario '-'");
            $$ = (($2 == TYPE_CHAR) ? TYPE_INT : $2);
        }
    |   LPAREN expr RPAREN
        {
            $$ = $2;
        }
    |   IDENTIFIER
        {
            Symbol *symbol = lookup_variable($1);
            $$ = symbol ? symbol->type : TYPE_UNKNOWN;
        }
    |   IDENTIFIER LPAREN args_opt RPAREN
        {
            Symbol *symbol = lookup_function($1, $3);
            $$ = symbol ? symbol->type : TYPE_UNKNOWN;
        }
    |   INTEGER_LITERAL
        {
            $$ = TYPE_INT;
        }
    |   REAL_LITERAL
        {
            $$ = TYPE_DOUBLE;
        }
    |   CHAR_LITERAL
        {
            $$ = TYPE_CHAR;
        }
    |   TRUE
        {
            $$ = TYPE_BOOLEAN;
        }
    |   FALSE
        {
            $$ = TYPE_BOOLEAN;
        }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "ERRO DE SINTAXE na linha %d: %s\n", yylineno, s);
}
