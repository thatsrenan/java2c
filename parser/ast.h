#ifndef AST_H
#define AST_H

// Tipos da linguagem
// TYPE_UNKNOWN vem primeiro de proposito: new_node usa calloc, entao todo
// no nasce com type = TYPE_UNKNOWN ate o semantico preencher.
 * TYPE_STRING NAO e tipo de variavel (spec 1.4): so descreve o literal
 * de string do println. */
typedef enum {
    TYPE_UNKNOWN,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BOOLEAN,
    TYPE_CHAR,
    TYPE_VOID,      /* so como tipo de retorno de metodo */
    TYPE_STRING,    /* so para NODE_STR */
    TYPE_ERROR
} Type;

// Operadores (independentes dos tokens do Bison)
typedef enum {
    /* aritmeticos */
    OP_ADD, OP_SUB, OP_MULT, OP_DIV, OP_MOD,
    /* relacionais */
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,
    /* logicos */
    OP_AND, OP_OR,
    /* unarios */
    OP_NOT, OP_NEG,
    /* atribuicao */
    OP_ASSIGN, OP_ADD_ASSIGN, OP_SUB_ASSIGN, OP_MULT_ASSIGN, OP_DIV_ASSIGN,
    /* incremento / decremento */
    OP_PREINC, OP_POSTINC, OP_PREDEC, OP_POSTDEC
} OpKind;

// Etiqueta de cada no
typedef enum {
    /* expressoes */
    NODE_INT, NODE_FLOAT, NODE_CHAR, NODE_STR, NODE_BOOL,
    NODE_IDENTIFIER,
    NODE_BINOP,
    NODE_UNARY,
    NODE_CALL,
    NODE_CAST,
    /* statements */
    NODE_ASSIGN,
    NODE_VARDECL,
    NODE_INCDEC,
    NODE_PRINT,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_DOWHILE,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_RETURN,
    NODE_BLOCK,
    /* estrutura do programa */
    NODE_PARAM,
    NODE_METHOD,
    NODE_CLASS
} NodeKind;

// Convencoes dos campos
 *  NODE_INT       : val.i = valor
 *  NODE_FLOAT     : val.d = valor (sufixo f/d nao e distinguido pelo lexer)
 *  NODE_CHAR      : name  = texto cru entre aspas simples (ex.: "a" ou "\n")
 *  NODE_STR       : name  = texto cru sem as aspas (so filho de NODE_PRINT)
 *  NODE_BOOL      : val.i = 1 (true) ou 0 (false)
 *  NODE_IDENTIFIER: name  = nome da variavel
 *  NODE_BINOP     : op = operador, a = esquerda, b = direita
 *  NODE_UNARY     : op = OP_NOT ou OP_NEG, a = operando
 *  NODE_CALL      : name = metodo chamado,
 *                   a = lista de argumentos (expressoes ligadas por next) ou NULL
 *  NODE_CAST      : decl_type = tipo de destino, a = operando
 *
 *  NODE_ASSIGN    : op = OP_ASSIGN/OP_*_ASSIGN, name = variavel, a = expressao
 *  NODE_VARDECL   : decl_type = tipo declarado, name = variavel,
 *                   a = valor inicial ou NULL,
 *                   is_global = 1 se for campo static da classe
 *  NODE_INCDEC    : op = OP_PREINC/POSTINC/PREDEC/POSTDEC, name = variavel
 *  NODE_PRINT     : a = expressao ou NODE_STR
 *  NODE_IF        : a = condicao, b = bloco then,
 *                   c = bloco else, outro NODE_IF (else if) ou NULL
 *  NODE_WHILE     : a = condicao, b = corpo
 *  NODE_FOR       : a = init (VARDECL/ASSIGN/NULL), b = condicao (ou NULL),
 *                   c = update (ASSIGN/INCDEC/NULL), d = corpo
 *  NODE_DOWHILE   : a = corpo, b = condicao
 *  NODE_BREAK     : sem filhos
 *  NODE_CONTINUE  : sem filhos
 *  NODE_RETURN    : a = expressao ou NULL
 *  NODE_BLOCK     : a = primeiro statement da lista (ligados por next)
 *
 *  NODE_PARAM     : decl_type = tipo, name = nome; parametros ligados por next
 *  NODE_METHOD    : decl_type = tipo de retorno, name = nome,
 *                   a = lista de NODE_PARAM (ou NULL),
 *                   b = corpo (NODE_BLOCK)
 *                   (o "String[] args" do main NAO vira parametro: o parser
 *                    o descarta, entao main tem a = NULL)
 *  NODE_CLASS     : name = nome da classe,
 *                   a = membros (NODE_VARDECL global e NODE_METHOD, ligados por next)
 *
 *  next = proximo irmao em uma lista (statements, membros, parametros,
 *         argumentos). Nunca e filho estrutural.
 *  Cada no tem um unico dono: nao reutilize um mesmo no em dois lugares.
 *  Strings em 'name' pertencem ao no (vem de strdup no lexer).
 */

typedef struct Node {
    NodeKind kind;
    int      line;
    Type     type;        /* tipo da expressao: o semantico preenche */
    Type     decl_type;   /* tipo declarado / retorno / destino do cast */
    OpKind   op;
    int      is_global;   /* 1 em NODE_VARDECL de campo static */
    char    *name;
    union {
        long   i;
        double d;
    } val;
    struct Node *a, *b, *c, *d;   /* filhos estruturais */
    struct Node *next;            /* proximo irmao da lista */
} Node;

// Construcao basica
Node *new_node(NodeKind kind, int line);
Node *append(Node *list, Node *n);

//  Construtores auxiliares (deixam as acoes do Bison curtas)
 * Os que recebem 'char *' ASSUMEM A POSSE da string (nao fazem strdup). */
Node *new_int(long v, int line);
Node *new_real(double v, int line);
Node *new_char(char *text, int line);
Node *new_str(char *text, int line);
Node *new_bool(int v, int line);
Node *new_ident(char *name, int line);
Node *new_binop(OpKind op, Node *l, Node *r, int line);
Node *new_unary(OpKind op, Node *operand, int line);
Node *new_call(char *name, Node *args, int line);
Node *new_cast(Type target, Node *operand, int line);
Node *new_assign(OpKind op, char *name, Node *expr, int line);
Node *new_vardecl(Type t, char *name, Node *init, int line);
Node *new_field(Type t, char *name, Node *init, int line);   /* vardecl global */
Node *new_incdec(OpKind op, char *name, int line);
Node *new_param(Type t, char *name, int line);
Node *new_method(Type ret, char *name, Node *params, Node *body, int line);
Node *new_class(char *name, Node *members, int line);

// Utilitarios
void        print_ast(Node *n, int indent);
void        free_ast(Node *n);
const char *type_name(Type t);
const char *op_name(OpKind op);

extern Node *ast_root;

#endif