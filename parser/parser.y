/*
 * parser.y
 *
 * Analisador sintático (Bison) do compilador Java -> C, com:
 *   - construção da AST (parser/ast.h) a partir da gramática de
 *     docs/definicao-escopo-sintaxe-semantica.md;
 *   - análise de escopo/declaração/tipos usando a tabela de símbolos
 *     em src/symbol_table.c.
 *
 * Cobre o subconjunto procedural da spec (seções 12-17):
 *   - classe [public] class Nome { membros } e programa solto;
 *   - campos static (globais em C) e métodos static;
 *   - declaração de variável (int, float, double, boolean, char);
 *   - atribuição (=, +=, -=, *=, /=);
 *   - expressões aritméticas, relacionais, lógicas, chamadas de método
 *     e cast explícito ((int) x);
 *   - if / else if / else; while, for, do-while, break, continue, return;
 *   - incremento/decremento (i++, ++i, i--, --i);
 *   - blocos { ... } com escopo próprio;
 *   - construção simplificada System.out.println(...).
 *
 * "String" só aparece em "String[] args" e literais de string só dentro
 * do println (spec 1.4); qualquer outro uso é rejeitado com mensagem
 * própria. "void" só como retorno de método.
 *
 * Além da forma com classe, o programa pode vir "solto": sequência de
 * campos static, métodos e statements sem a classe em volta. É a forma
 * usada pelos casos de examples/escopo e por examples/exemplo1.java.
 *
 * As ações montam a AST (parser/ast.h) e, ao final de uma análise
 * sem erros, a raiz fica em ast_root. A linha de cada nó vem da
 * posição do token que o identifica (%locations; o lexer preenche
 * yylloc).
 *
 * Erros semânticos são reportados como "ERRO SEMANTICO na linha N: ..."
 * e marcam had_semantic_error, que main.c consulta para o status
 * final (não declaração de sucesso quando há erro semântico).
 */

%code requires {
#include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "symbol_table.h"

int yylex(void);
void yyerror(const char *s);
extern int yylineno;

/* main.c consulta esta flag para não declarar sucesso quando houve erro. */
int had_semantic_error = 0;

/* main.c ajusta esta flag quando o usuário passa --tabela. */
int show_symbol_table = 0;

/* Contexto do método sendo analisado (não há métodos aninhados). */
Symbol *current_function = NULL;
DataType current_return_type = ST_TYPE_VOID;

/* Profundidade de laços, para validar break/continue. */
int loop_depth = 0;

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
/* Verificações de tipo (DataType, da tabela de símbolos)             */
/* ------------------------------------------------------------------ */

static int is_numeric(DataType type) {
    return type == ST_TYPE_INT || type == ST_TYPE_FLOAT ||
           type == ST_TYPE_DOUBLE || type == ST_TYPE_CHAR;
}

static int is_integral(DataType type) {
    return type == ST_TYPE_INT || type == ST_TYPE_CHAR;
}

static int is_assignable(DataType dst, DataType src) {
    if (dst == ST_TYPE_UNKNOWN || src == ST_TYPE_UNKNOWN) return 1;
    if (dst == src) return 1;
    if (dst == ST_TYPE_DOUBLE &&
        (src == ST_TYPE_FLOAT || src == ST_TYPE_INT || src == ST_TYPE_CHAR)) return 1;
    if (dst == ST_TYPE_FLOAT && (src == ST_TYPE_INT || src == ST_TYPE_CHAR)) return 1;
    if (dst == ST_TYPE_INT && src == ST_TYPE_CHAR) return 1;
    return 0;
}

static DataType promote(DataType a, DataType b) {
    if (a == ST_TYPE_UNKNOWN || b == ST_TYPE_UNKNOWN) return ST_TYPE_UNKNOWN;
    if (a == ST_TYPE_DOUBLE || b == ST_TYPE_DOUBLE) return ST_TYPE_DOUBLE;
    if (a == ST_TYPE_FLOAT || b == ST_TYPE_FLOAT) return ST_TYPE_FLOAT;
    return ST_TYPE_INT;
}

static void require_numeric(DataType type, const char *context) {
    if (type != ST_TYPE_UNKNOWN && !is_numeric(type)) {
        semantic_error("%s exige tipo numerico (tipo encontrado: %s)",
                       context, st_type_name(type));
    }
}

static void require_integral(DataType type, const char *context) {
    if (type != ST_TYPE_UNKNOWN && !is_integral(type)) {
        semantic_error("%s exige tipo inteiro (tipo encontrado: %s)",
                       context, st_type_name(type));
    }
}

static void require_boolean(DataType type, const char *context) {
    if (type != ST_TYPE_UNKNOWN && type != ST_TYPE_BOOLEAN) {
        semantic_error("%s deve possuir tipo boolean (tipo encontrado: %s)",
                       context, st_type_name(type));
    }
}

static void check_comparable(DataType a, DataType b, const char *context) {
    if (a == ST_TYPE_UNKNOWN || b == ST_TYPE_UNKNOWN) return;
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

/* Converte Type (ast.h) para DataType (symbol_table.h). */
static DataType dt_from_type(Type t) {
    switch (t) {
        case TYPE_INT:     return ST_TYPE_INT;
        case TYPE_FLOAT:   return ST_TYPE_FLOAT;
        case TYPE_DOUBLE:  return ST_TYPE_DOUBLE;
        case TYPE_BOOLEAN: return ST_TYPE_BOOLEAN;
        case TYPE_CHAR:    return ST_TYPE_CHAR;
        case TYPE_VOID:    return ST_TYPE_VOID;
        case TYPE_STRING:  return ST_TYPE_STRING;
        default:           return ST_TYPE_UNKNOWN;
    }
}

/* Tipo estático (DataType) de um nó de expressão da AST, sem reemitir
 * erros já emitidos na criação dos filhos (ex.: variável não declarada). */
static DataType dt_of(Node *n) {
    if (!n) return ST_TYPE_UNKNOWN;
    switch (n->kind) {
        case NODE_INT:   return ST_TYPE_INT;
        case NODE_FLOAT: return ST_TYPE_DOUBLE;
        case NODE_CHAR:  return ST_TYPE_CHAR;
        case NODE_BOOL:  return ST_TYPE_BOOLEAN;
        case NODE_STR:   return ST_TYPE_STRING;
        case NODE_IDENTIFIER: {
            Symbol *s = st_lookup(n->name);
            if (!s || s->kind == SYM_FUNCTION) return ST_TYPE_UNKNOWN;
            return s->type;
        }
        case NODE_CALL: {
            Symbol *s = st_lookup_global(n->name);
            if (!s || s->kind != SYM_FUNCTION) return ST_TYPE_UNKNOWN;
            return s->type;
        }
        case NODE_CAST: return dt_from_type(n->decl_type);
        case NODE_UNARY: {
            if (n->op == OP_NOT) return ST_TYPE_BOOLEAN;
            if (n->op == OP_NEG) {
                DataType d = dt_of(n->a);
                return d == ST_TYPE_CHAR ? ST_TYPE_INT : d;
            }
            return ST_TYPE_UNKNOWN;
        }
        case NODE_BINOP: {
            switch (n->op) {
                case OP_ADD: case OP_SUB: case OP_MULT: case OP_DIV:
                    return promote(dt_of(n->a), dt_of(n->b));
                case OP_MOD: return ST_TYPE_INT;
                case OP_LT: case OP_GT: case OP_LE: case OP_GE:
                case OP_EQ: case OP_NE: case OP_AND: case OP_OR:
                    return ST_TYPE_BOOLEAN;
                default: return ST_TYPE_UNKNOWN;
            }
        }
        default: return ST_TYPE_UNKNOWN;
    }
}

static void check_assignment_op(Symbol *var, OpKind op, DataType rhs) {
    if (!var) return;
    const char *opstr = op_name(op);
    if (op == OP_ASSIGN) {
        if (!is_assignable(var->type, rhs)) {
            semantic_error("tipos incompativeis: nao e possivel atribuir %s a '%s' (%s)",
                           st_type_name(rhs), var->name, st_type_name(var->type));
        }
        return;
    }
    if (var->type != ST_TYPE_UNKNOWN && !is_numeric(var->type)) {
        semantic_error("o operador '%s' exige uma variavel numerica ('%s' e %s)",
                       opstr, var->name, st_type_name(var->type));
    }
    if (rhs != ST_TYPE_UNKNOWN && !is_numeric(rhs)) {
        semantic_error("o operador '%s' exige um valor numerico (recebeu %s)",
                       opstr, st_type_name(rhs));
    }
}

static void check_increment(Symbol *var, const char *op) {
    if (!var) return;
    if (var->type != ST_TYPE_UNKNOWN && !is_numeric(var->type)) {
        semantic_error("o operador '%s' exige uma variavel numerica ('%s' e %s)",
                       op, var->name, st_type_name(var->type));
    }
}

/* ------------------------------------------------------------------ */
/* Declaração e busca de símbolos                                     */
/* ------------------------------------------------------------------ */

static Symbol *declare_symbol(const char *name, DataType type,
                              SymbolKind kind, int line) {
    Symbol *symbol = NULL;
    switch (kind) {
        case SYM_VARIABLE:
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
        semantic_error("a variavel '%s' ja foi declarada neste escopo", name);
    }
    return symbol;
}

static Symbol *declare_variable(const char *name, DataType type, int line, int initialized) {
    Symbol *symbol;
    if (type == ST_TYPE_VOID) {
        semantic_error("a variavel '%s' nao pode ter tipo void", name);
    } else if (type == ST_TYPE_STRING) {
        semantic_error("o tipo String nao e suportado pelo subconjunto Java");
    }
    SymbolKind kind = st_is_global_scope() ? SYM_GLOBAL : SYM_VARIABLE;
    symbol = declare_symbol(name, type, kind, line);
    if (symbol && initialized) st_mark_initialized(symbol);
    return symbol;
}

static Symbol *declare_global(const char *name, DataType type, int line, int initialized) {
    Symbol *symbol;
    if (type == ST_TYPE_VOID) {
        semantic_error("o campo '%s' nao pode ter tipo void", name);
    } else if (type == ST_TYPE_STRING) {
        semantic_error("o tipo String nao e suportado pelo subconjunto Java");
    }
    symbol = declare_symbol(name, type, SYM_GLOBAL, line);
    if (symbol && initialized) st_mark_initialized(symbol);
    return symbol;
}

static Symbol *declare_function(const char *name, DataType return_type, int line) {
    Symbol *symbol;
    if (return_type == ST_TYPE_STRING) {
        semantic_error("o tipo String nao e suportado como retorno de metodo");
    }
    symbol = declare_symbol(name, return_type, SYM_FUNCTION, line);
    if (symbol) st_mark_initialized(symbol);
    return symbol;
}

static Symbol *declare_parameter(const char *name, DataType type, int line, int allow_string_array) {
    Symbol *symbol;
    if (type == ST_TYPE_VOID) {
        semantic_error("o parametro '%s' nao pode ter tipo void", name);
    } else if (type == ST_TYPE_STRING && !allow_string_array) {
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

/* Pilha de chamadas em andamento (uma chamada pode conter outra: f(g(1))) */
#define MAX_CALL_DEPTH 64
typedef struct {
    Symbol *fn;
    int argc;
} CallFrame;
static CallFrame call_stack[MAX_CALL_DEPTH];
int call_depth = 0;

static void call_begin(const char *name) {
    Symbol *symbol = st_lookup_global(name);
    if (!symbol) {
        semantic_error("a funcao '%s' nao foi declarada antes do uso", name);
    } else if (symbol->kind != SYM_FUNCTION) {
        semantic_error("'%s' nao e uma funcao", name);
        symbol = NULL;
    }
    if (call_depth < MAX_CALL_DEPTH) {
        call_stack[call_depth].fn = symbol;
        call_stack[call_depth].argc = 0;
    }
    call_depth++;
}

static void call_arg(DataType type) {
    if (call_depth == 0 || call_depth > MAX_CALL_DEPTH) return;
    CallFrame *frame = &call_stack[call_depth - 1];
    int index = frame->argc++;
    if (frame->fn && index < frame->fn->param_count &&
        !is_assignable(frame->fn->param_types[index], type)) {
        semantic_error("argumento %d de '%s': esperado %s, mas recebeu %s",
                       index + 1, frame->fn->name,
                       st_type_name(frame->fn->param_types[index]),
                       st_type_name(type));
    }
}

static Symbol *call_end(void) {
    Symbol *fn = NULL;
    call_depth--;
    if (call_depth >= 0 && call_depth < MAX_CALL_DEPTH) {
        CallFrame *frame = &call_stack[call_depth];
        fn = frame->fn;
        if (fn && frame->argc != fn->param_count) {
            semantic_error("a funcao '%s' espera %d argumento(s), mas recebeu %d",
                           fn->name, fn->param_count, frame->argc);
        }
    }
    return fn;
}
%}

/* %code (sem qualificador) sai depois do %code requires, entao ja enxerga Node */
%code {
/* 1 quando o ultimo param_opt reduzido foi o "String[] args" do main.
 * Seguro como global: métodos não se aninham, então nenhum outro
 * param_opt é reduzido entre os parâmetros e o fim do próprio método. */
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
 * as subárvores e strings já montadas vazariam. ('program' não tem tipo
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
        class_decl
        {
            ast_root = $1;
            if (show_symbol_table) st_print(stdout);
        }
    |   { st_enter_scope(); current_return_type = ST_TYPE_VOID; } top_list
        {
            if (show_symbol_table) st_print(stdout);
            st_exit_scope();
            ast_root = new_class(NULL, $2, 1);
        }
    |   /* vazio */
        { ast_root = new_class(NULL, NULL, 1); }
    ;

/* ================= Estrutura do programa ================= */

/*
 * PUBLIC aparece explícito nas alternativas (em vez de um "opt_public"
 * vazio) para não gerar conflito: num programa solto, ao ver STATIC o
 * parser não saberia se reduz o opcional vazio (método) ou não (campo).
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
        {
            declare_global($3, dt_from_type($2), @3.first_line, 0);
            $$ = new_field($2, $3, NULL, @3.first_line);
        }
    |   STATIC type IDENTIFIER ASSIGN expr SEMICOLON
        {
            Symbol *sym = declare_global($3, dt_from_type($2), @3.first_line, 1);
            if (sym) {
                DataType rhs = dt_of($5);
                if (!is_assignable(sym->type, rhs)) {
                    semantic_error("tipos incompativeis no inicializador de '%s' (%s <- %s)",
                                   sym->name, st_type_name(sym->type), st_type_name(rhs));
                }
            }
            $$ = new_field($2, $3, $5, @3.first_line);
        }
    ;

method_decl:
        STATIC method_sig             { $$ = $2; }
    |   PUBLIC STATIC method_sig      { $$ = $3; }
    ;

/*
 * O tipo de retorno fica dentro de method_sig (e não num "ret_type") para
 * que "static int x" só precise decidir entre campo e método ao ver o
 * token depois do nome: ';'/'=' (campo) ou '(' (método).
 *
 * A declaração da função e a entrada no escopo da função acontecem em
 * ação intermediária (antes dos parâmetros), para que:
 *   - a função seja visível dentro do próprio corpo (recursão);
 *   - parâmetros sejam inseridos no escopo da função.
 * Índices: type=$1 ID=$2 mid=$3 LPAREN=$4 param_opt=$5 RPAREN=$6 block=$7
 */
method_sig:
        type IDENTIFIER
        {
            Symbol *function = declare_function($2, dt_from_type($1), @2.first_line);
            current_function = function;
            current_return_type = dt_from_type($1);
            st_enter_function_scope();
        }
        LPAREN param_opt RPAREN block
        {
            st_exit_scope();
            current_function = NULL;
            current_return_type = ST_TYPE_VOID;
            if (params_were_main_args && strcmp($2, "main") != 0) {
                yyerror("\"String[] args\" so e permitido como parametro de main");
                /* YYERROR não libera os símbolos desta regra */
                free($2); free_ast($5); free_ast($7);
                YYERROR;
            }
            $$ = new_method($1, $2, $5, $7, @2.first_line);
        }
    |   VOID IDENTIFIER
        {
            Symbol *function = declare_function($2, ST_TYPE_VOID, @2.first_line);
            current_function = function;
            current_return_type = ST_TYPE_VOID;
            st_enter_function_scope();
        }
        LPAREN param_opt RPAREN block
        {
            st_exit_scope();
            current_function = NULL;
            current_return_type = ST_TYPE_VOID;
            if (params_were_main_args && strcmp($2, "main") != 0) {
                yyerror("\"String[] args\" so e permitido como parametro de main");
                free($2); free_ast($5); free_ast($7);
                YYERROR;
            }
            $$ = new_method(TYPE_VOID, $2, $5, $7, @2.first_line);
        }
    ;

/* "String[] args" não vira parâmetro (ast.h): main fica com a = NULL */
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
        type IDENTIFIER
        {
            declare_parameter($2, dt_from_type($1), @2.first_line, 0);
            $$ = new_param($1, $2, @2.first_line);
        }
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
        LBRACE
        { st_enter_scope(); }
        stmt_list
        RBRACE
        {
            st_exit_scope();
            $$ = new_node(NODE_BLOCK, @1.first_line);
            /* stmt_list é $3: LBRACE=$1 mid=$2 stmt_list=$3 RBRACE=$4 */
            $$->a = $3;
        }
    ;

type:
        INT          { $$ = TYPE_INT; }
    |   FLOAT        { $$ = TYPE_FLOAT; }
    |   DOUBLE       { $$ = TYPE_DOUBLE; }
    |   BOOLEAN      { $$ = TYPE_BOOLEAN; }
    |   CHAR         { $$ = TYPE_CHAR; }
        /* String não é tipo do subconjunto (spec 1.4 / 13.2): reconhecido
         * aqui só para dar uma mensagem clara em vez de "syntax error" */
    |   STRING_TYPE
        {
            yyerror("o tipo String nao e suportado pelo subconjunto Java");
            YYERROR;
        }
    ;

vardecl:
        type IDENTIFIER SEMICOLON
        {
            declare_variable($2, dt_from_type($1), @2.first_line, 0);
            $$ = new_vardecl($1, $2, NULL, @2.first_line);
        }
    |   type IDENTIFIER ASSIGN expr SEMICOLON
        {
            Symbol *sym = declare_variable($2, dt_from_type($1), @2.first_line, 1);
            if (sym) {
                DataType rhs = dt_of($4);
                if (!is_assignable(sym->type, rhs)) {
                    semantic_error("tipos incompativeis no inicializador de '%s' (%s <- %s)",
                                   sym->name, st_type_name(sym->type), st_type_name(rhs));
                }
            }
            $$ = new_vardecl($1, $2, $4, @2.first_line);
        }
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
        {
            Symbol *sym = lookup_variable($1);
            check_assignment_op(sym, $2, dt_of($3));
            $$ = new_assign($2, $1, $3, @1.first_line);
        }
    ;

increment_stmt:
        IDENTIFIER INCREMENT SEMICOLON
        {
            check_increment(lookup_variable($1), "++");
            $$ = new_incdec(OP_POSTINC, $1, @1.first_line);
        }
    |   IDENTIFIER DECREMENT SEMICOLON
        {
            check_increment(lookup_variable($1), "--");
            $$ = new_incdec(OP_POSTDEC, $1, @1.first_line);
        }
    |   INCREMENT IDENTIFIER SEMICOLON
        {
            check_increment(lookup_variable($2), "++");
            $$ = new_incdec(OP_PREINC, $2, @2.first_line);
        }
    |   DECREMENT IDENTIFIER SEMICOLON
        {
            check_increment(lookup_variable($2), "--");
            $$ = new_incdec(OP_PREDEC, $2, @2.first_line);
        }
    ;

/* chamada de método como statement: f(a, b); */
call_stmt:
        call SEMICOLON                    { $$ = $1; }
    ;

call:
        IDENTIFIER LPAREN
        { call_begin($1); }
        arg_opt RPAREN
        {
            /* IDENTIFIER=$1 LPAREN=$2 mid=$3 arg_opt=$4 RPAREN=$5 */
            call_end();
            $$ = new_call($1, $4, @1.first_line);
        }
    ;

arg_opt:
        /* vazio */                       { $$ = NULL; }
    |   arg_list                          { $$ = $1; }
    ;

arg_list:
        expr
        {
            DataType t = dt_of($1);
            if (t == ST_TYPE_VOID) semantic_error("nao e possivel passar uma expressao void como argumento");
            call_arg(t);
            $$ = $1;
        }
    |   arg_list COMMA expr
        {
            DataType t = dt_of($3);
            if (t == ST_TYPE_VOID) semantic_error("nao e possivel passar uma expressao void como argumento");
            call_arg(t);
            $$ = append($1, $3);
        }
    ;

/*
 * Construção simplificada equivalente a System.out.println(...).
 * O ponto (.) é reconhecido como token separado pelo lexer; é o parser
 * quem reconhece a sequência System . out . println ( ... ) ;
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
            if (dt_of($7) == ST_TYPE_VOID) {
                semantic_error("nao e possivel imprimir uma expressao de tipo void");
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
            require_boolean(dt_of($3), "a condicao do if");
            $$ = new_node(NODE_IF, @1.first_line);
            $$->a = $3; $$->b = $5;
        }
    |   IF LPAREN expr RPAREN block ELSE block
        {
            require_boolean(dt_of($3), "a condicao do if");
            $$ = new_node(NODE_IF, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = $7;
        }
    |   IF LPAREN expr RPAREN block ELSE if_stmt
        {
            require_boolean(dt_of($3), "a condicao do if");
            $$ = new_node(NODE_IF, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = $7;
        }
    ;

while_stmt:
        WHILE LPAREN expr RPAREN
        {
            require_boolean(dt_of($3), "a condicao do while");
            loop_depth++;
        }
        block
        {
            /* WHILE=$1 LPAREN=$2 expr=$3 RPAREN=$4 mid=$5 block=$6 */
            loop_depth--;
            $$ = new_node(NODE_WHILE, @1.first_line);
            $$->a = $3; $$->b = $6;
        }
    ;

/*
 * for ( init ; condicao ; atualizacao ) bloco
 * Os três campos são opcionais, como na spec ([<init>] ; [<cond>] ; [<inc>]).
 * A variável declarada no init só existe dentro do for.
 * Índices: FOR=$1 LPAREN=$2 mid1=$3 for_init=$4 SEMI=$5 for_cond=$6
 *          SEMI=$7 for_update=$8 RPAREN=$9 mid2=$10 block=$11
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
            $$ = new_node(NODE_FOR, @1.first_line);
            $$->a = $4; $$->b = $6; $$->c = $8; $$->d = $11;
        }
    ;

for_init:
        /* vazio */                       { $$ = NULL; }
    |   type IDENTIFIER ASSIGN expr
        {
            Symbol *sym = declare_variable($2, dt_from_type($1), @2.first_line, 1);
            if (sym) {
                DataType rhs = dt_of($4);
                if (!is_assignable(sym->type, rhs)) {
                    semantic_error("tipos incompativeis no inicializador de '%s' (%s <- %s)",
                                   sym->name, st_type_name(sym->type), st_type_name(rhs));
                }
            }
            $$ = new_vardecl($1, $2, $4, @2.first_line);
        }
    |   IDENTIFIER assign_op expr
        {
            Symbol *sym = lookup_variable($1);
            check_assignment_op(sym, $2, dt_of($3));
            $$ = new_assign($2, $1, $3, @1.first_line);
        }
    ;

for_cond:
        /* vazio */                       { $$ = NULL; }
    |   expr
        {
            require_boolean(dt_of($1), "a condicao do for");
            $$ = $1;
        }
    ;

for_update:
        /* vazio */                       { $$ = NULL; }
    |   IDENTIFIER assign_op expr
        {
            Symbol *sym = lookup_variable($1);
            check_assignment_op(sym, $2, dt_of($3));
            $$ = new_assign($2, $1, $3, @1.first_line);
        }
    |   IDENTIFIER INCREMENT
        {
            check_increment(lookup_variable($1), "++");
            $$ = new_incdec(OP_POSTINC, $1, @1.first_line);
        }
    |   IDENTIFIER DECREMENT
        {
            check_increment(lookup_variable($1), "--");
            $$ = new_incdec(OP_POSTDEC, $1, @1.first_line);
        }
    |   INCREMENT IDENTIFIER
        {
            check_increment(lookup_variable($2), "++");
            $$ = new_incdec(OP_PREINC, $2, @2.first_line);
        }
    |   DECREMENT IDENTIFIER
        {
            check_increment(lookup_variable($2), "--");
            $$ = new_incdec(OP_PREDEC, $2, @2.first_line);
        }
    ;

do_while_stmt:
        DO
        { loop_depth++; }
        block
        { loop_depth--; }
        WHILE LPAREN expr RPAREN SEMICOLON
        {
            /* DO=$1 mid1=$2 block=$3 mid2=$4 WHILE=$5 LPAREN=$6 expr=$7 RPAREN=$8 SEMI=$9 */
            require_boolean(dt_of($7), "a condicao do do-while");
            $$ = new_node(NODE_DOWHILE, @1.first_line);
            $$->a = $3; $$->b = $7;
        }
    ;

break_stmt:
        BREAK SEMICOLON
        {
            if (loop_depth == 0) {
                semantic_error("'break' so pode ser usado dentro de um laco");
            }
            $$ = new_node(NODE_BREAK, @1.first_line);
        }
    ;

continue_stmt:
        CONTINUE SEMICOLON
        {
            if (loop_depth == 0) {
                semantic_error("'continue' so pode ser usado dentro de um laco");
            }
            $$ = new_node(NODE_CONTINUE, @1.first_line);
        }
    ;

return_stmt:
        RETURN SEMICOLON
        {
            if (current_return_type != ST_TYPE_VOID) {
                semantic_error("o metodo deve retornar um valor do tipo %s",
                               st_type_name(current_return_type));
            }
            $$ = new_node(NODE_RETURN, @1.first_line);
        }
    |   RETURN expr SEMICOLON
        {
            DataType t = dt_of($2);
            if (current_return_type == ST_TYPE_VOID) {
                semantic_error("metodo void nao pode retornar valor");
            } else if (!is_assignable(current_return_type, t)) {
                semantic_error("tipo de retorno incompativel (%s esperado, %s encontrado)",
                               st_type_name(current_return_type), st_type_name(t));
            }
            $$ = new_node(NODE_RETURN, @1.first_line);
            $$->a = $2;
        }
    ;

/* A linha de um operador binário é a do próprio operador (@2). */
expr:
        expr PLUS expr
        {
            check_arithmetic(dt_of($1), dt_of($3), "+");
            $$ = new_binop(OP_ADD, $1, $3, @2.first_line);
        }
    |   expr MINUS expr
        {
            check_arithmetic(dt_of($1), dt_of($3), "-");
            $$ = new_binop(OP_SUB, $1, $3, @2.first_line);
        }
    |   expr MULTIPLY expr
        {
            check_arithmetic(dt_of($1), dt_of($3), "*");
            $$ = new_binop(OP_MULT, $1, $3, @2.first_line);
        }
    |   expr DIVIDE expr
        {
            check_arithmetic(dt_of($1), dt_of($3), "/");
            $$ = new_binop(OP_DIV, $1, $3, @2.first_line);
        }
    |   expr MODULO expr
        {
            require_integral(dt_of($1), "o operador '%'");
            require_integral(dt_of($3), "o operador '%'");
            $$ = new_binop(OP_MOD, $1, $3, @2.first_line);
        }
    |   expr LESS expr
        {
            require_numeric(dt_of($1), "o operador '<'");
            require_numeric(dt_of($3), "o operador '<'");
            $$ = new_binop(OP_LT, $1, $3, @2.first_line);
        }
    |   expr GREATER expr
        {
            require_numeric(dt_of($1), "o operador '>'");
            require_numeric(dt_of($3), "o operador '>'");
            $$ = new_binop(OP_GT, $1, $3, @2.first_line);
        }
    |   expr LESS_EQUAL expr
        {
            require_numeric(dt_of($1), "o operador '<='");
            require_numeric(dt_of($3), "o operador '<='");
            $$ = new_binop(OP_LE, $1, $3, @2.first_line);
        }
    |   expr GREATER_EQUAL expr
        {
            require_numeric(dt_of($1), "o operador '>='");
            require_numeric(dt_of($3), "o operador '>='");
            $$ = new_binop(OP_GE, $1, $3, @2.first_line);
        }
    |   expr EQUAL expr
        {
            check_comparable(dt_of($1), dt_of($3), "o operador '=='");
            $$ = new_binop(OP_EQ, $1, $3, @2.first_line);
        }
    |   expr NOT_EQUAL expr
        {
            check_comparable(dt_of($1), dt_of($3), "o operador '!='");
            $$ = new_binop(OP_NE, $1, $3, @2.first_line);
        }
    |   expr AND expr
        {
            require_boolean(dt_of($1), "o operador '&&'");
            require_boolean(dt_of($3), "o operador '&&'");
            $$ = new_binop(OP_AND, $1, $3, @2.first_line);
        }
    |   expr OR expr
        {
            require_boolean(dt_of($1), "o operador '||'");
            require_boolean(dt_of($3), "o operador '||'");
            $$ = new_binop(OP_OR, $1, $3, @2.first_line);
        }
    |   NOT expr
        {
            require_boolean(dt_of($2), "o operador '!'");
            $$ = new_unary(OP_NOT, $2, @1.first_line);
        }
    |   MINUS expr %prec UMINUS
        {
            require_numeric(dt_of($2), "o operador unario '-'");
            $$ = new_unary(OP_NEG, $2, @1.first_line);
        }
    |   LPAREN expr RPAREN       { $$ = $2; }
    |   LPAREN type RPAREN expr %prec UMINUS
                                 { $$ = new_cast($2, $4, @1.first_line); }
    |   call                     { $$ = $1; }
    |   IDENTIFIER
        {
            lookup_variable($1);
            $$ = new_ident($1, @1.first_line);
        }
    |   INTEGER_LITERAL          { $$ = new_int($1, @1.first_line); }
    |   REAL_LITERAL             { $$ = new_real($1, @1.first_line); }
    |   CHAR_LITERAL             { $$ = new_char($1, @1.first_line); }
    |   TRUE                     { $$ = new_bool(1, @1.first_line); }
    |   FALSE                    { $$ = new_bool(0, @1.first_line); }
    ;

%%

/*
 * Programa solto: campos e métodos viram membros da classe implícita;
 * statements vão, na ordem, para um único NODE_BLOCK membro, criado onde
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

