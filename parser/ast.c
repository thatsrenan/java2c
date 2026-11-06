#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

Node *ast_root = NULL;
int   ast_print_lines = 0;

/* ================= Construcao ================= */

Node *new_node(NodeKind kind, int line) {
    Node *n = calloc(1, sizeof *n);   /* tudo zerado: ponteiros NULL, type UNKNOWN */
    if (!n) {
        fprintf(stderr, "ERRO: sem memoria ao criar no da AST.\n");
        exit(1);
    }
    n->kind = kind;
    n->line = line;
    return n;
}

Node *append(Node *list, Node *n) {
    if (!list) return n;
    if (!n)    return list;
    Node *p = list;
    while (p->next) p = p->next;
    p->next = n;                      /* se n ja tem next, a cadeia inteira vem junto */
    return list;
}

Node *new_int(long v, int line)       { Node *n = new_node(NODE_INT, line);        n->val.i = v; return n; }
Node *new_real(double v, int line)    { Node *n = new_node(NODE_FLOAT, line);      n->val.d = v; return n; }
Node *new_char(char *text, int line)  { Node *n = new_node(NODE_CHAR, line);       n->name = text; return n; }
Node *new_str(char *text, int line)   { Node *n = new_node(NODE_STR, line);        n->name = text; n->type = TYPE_STRING; return n; }
Node *new_bool(int v, int line)       { Node *n = new_node(NODE_BOOL, line);       n->val.i = v ? 1 : 0; return n; }
Node *new_ident(char *name, int line) { Node *n = new_node(NODE_IDENTIFIER, line); n->name = name; return n; }

Node *new_binop(OpKind op, Node *l, Node *r, int line) {
    Node *n = new_node(NODE_BINOP, line);
    n->op = op; n->a = l; n->b = r;
    return n;
}

Node *new_unary(OpKind op, Node *operand, int line) {
    Node *n = new_node(NODE_UNARY, line);
    n->op = op; n->a = operand;
    return n;
}

Node *new_call(char *name, Node *args, int line) {
    Node *n = new_node(NODE_CALL, line);
    n->name = name; n->a = args;
    return n;
}

Node *new_cast(Type target, Node *operand, int line) {
    Node *n = new_node(NODE_CAST, line);
    n->decl_type = target; n->a = operand;
    return n;
}

Node *new_assign(OpKind op, char *name, Node *expr, int line) {
    Node *n = new_node(NODE_ASSIGN, line);
    n->op = op; n->name = name; n->a = expr;
    return n;
}

Node *new_vardecl(Type t, char *name, Node *init, int line) {
    Node *n = new_node(NODE_VARDECL, line);
    n->decl_type = t; n->name = name; n->a = init;
    return n;
}

Node *new_field(Type t, char *name, Node *init, int line) {
    Node *n = new_vardecl(t, name, init, line);
    n->is_global = 1;
    return n;
}

Node *new_incdec(OpKind op, char *name, int line) {
    Node *n = new_node(NODE_INCDEC, line);
    n->op = op; n->name = name;
    return n;
}

Node *new_param(Type t, char *name, int line) {
    Node *n = new_node(NODE_PARAM, line);
    n->decl_type = t; n->name = name;
    return n;
}

Node *new_method(Type ret, char *name, Node *params, Node *body, int line) {
    Node *n = new_node(NODE_METHOD, line);
    n->decl_type = ret; n->name = name; n->a = params; n->b = body;
    return n;
}

Node *new_class(char *name, Node *members, int line) {
    Node *n = new_node(NODE_CLASS, line);
    n->name = name; n->a = members;
    return n;
}

/* ================= Nomes legiveis ================= */

const char *type_name(Type t) {
    switch (t) {
    case TYPE_UNKNOWN: return "?";
    case TYPE_INT:     return "int";
    case TYPE_FLOAT:   return "float";
    case TYPE_DOUBLE:  return "double";
    case TYPE_BOOLEAN: return "boolean";
    case TYPE_CHAR:    return "char";
    case TYPE_VOID:    return "void";
    case TYPE_STRING:  return "String";
    case TYPE_ERROR:   return "<erro>";
    }
    return "?";
}

const char *op_name(OpKind op) {
    switch (op) {
    case OP_ADD: return "+";   case OP_SUB: return "-";
    case OP_MULT: return "*";  case OP_DIV: return "/";
    case OP_MOD: return "%";
    case OP_LT: return "<";    case OP_GT: return ">";
    case OP_LE: return "<=";   case OP_GE: return ">=";
    case OP_EQ: return "==";   case OP_NE: return "!=";
    case OP_AND: return "&&";  case OP_OR: return "||";
    case OP_NOT: return "!";   case OP_NEG: return "-(unario)";
    case OP_ASSIGN: return "=";       case OP_ADD_ASSIGN: return "+=";
    case OP_SUB_ASSIGN: return "-=";  case OP_MULT_ASSIGN: return "*=";
    case OP_DIV_ASSIGN: return "/=";
    case OP_PREINC: return "++(pre)";  case OP_POSTINC: return "(pos)++";
    case OP_PREDEC: return "--(pre)";  case OP_POSTDEC: return "(pos)--";
    }
    return "?";
}

/* ================= Impressao ================= */

static void indent_to(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

/* imprime um filho com rotulo; filho NULL aparece como (vazio) */
static void print_child(const char *label, Node *c, int indent) {
    indent_to(indent);
    printf("%s:\n", label);
    if (c) {
        print_ast(c, indent + 1);
    } else {
        indent_to(indent + 1);
        printf("(vazio)\n");
    }
}

static void print_header(Node *n) {
    switch (n->kind) {
    case NODE_INT:        printf("INT %ld", n->val.i); break;
    case NODE_FLOAT:      printf("REAL %g", n->val.d); break;
    case NODE_CHAR:       printf("CHAR '%s'", n->name); break;
    case NODE_STR:        printf("STR \"%s\"", n->name); break;
    case NODE_BOOL:       printf("BOOL %s", n->val.i ? "true" : "false"); break;
    case NODE_IDENTIFIER: printf("ID %s", n->name); break;
    case NODE_BINOP:      printf("BINOP %s", op_name(n->op)); break;
    case NODE_UNARY:      printf("UNARY %s", op_name(n->op)); break;
    case NODE_CALL:       printf("CALL %s", n->name); break;
    case NODE_CAST:       printf("CAST (%s)", type_name(n->decl_type)); break;
    case NODE_ASSIGN:     printf("ASSIGN %s %s", n->name, op_name(n->op)); break;
    case NODE_VARDECL:    printf("%s %s %s", n->is_global ? "FIELD(static)" : "VARDECL",
                                 type_name(n->decl_type), n->name); break;
    case NODE_INCDEC:     printf("INCDEC %s %s", n->name, op_name(n->op)); break;
    case NODE_PRINT:      printf("PRINT"); break;
    case NODE_IF:         printf("IF"); break;
    case NODE_WHILE:      printf("WHILE"); break;
    case NODE_FOR:        printf("FOR"); break;
    case NODE_DOWHILE:    printf("DO-WHILE"); break;
    case NODE_BREAK:      printf("BREAK"); break;
    case NODE_CONTINUE:   printf("CONTINUE"); break;
    case NODE_RETURN:     printf("RETURN"); break;
    case NODE_BLOCK:      printf("BLOCK"); break;
    case NODE_PARAM:      printf("PARAM %s %s", type_name(n->decl_type), n->name); break;
    case NODE_METHOD:     printf("METHOD %s %s", type_name(n->decl_type), n->name); break;
    case NODE_CLASS:      printf("CLASS %s", n->name ? n->name : "(sem classe)"); break;
    }
    /* depois do semantico, mostra o tipo descoberto nas expressoes */
    if (n->type != TYPE_UNKNOWN && n->kind != NODE_STR) printf("  : %s", type_name(n->type));
    if (ast_print_lines) printf("  @%d", n->line);
    printf("\n");
}

void print_ast(Node *n, int indent) {
    for (; n; n = n->next) {          /* irmaos: mesma indentacao */
        indent_to(indent);
        print_header(n);

        switch (n->kind) {            /* filhos: um nivel abaixo */
        case NODE_IF:
            print_child("cond", n->a, indent + 1);
            print_child("then", n->b, indent + 1);
            if (n->c) print_child("else", n->c, indent + 1);
            break;
        case NODE_WHILE:
            print_child("cond", n->a, indent + 1);
            print_child("corpo", n->b, indent + 1);
            break;
        case NODE_FOR:
            print_child("init", n->a, indent + 1);
            print_child("cond", n->b, indent + 1);
            print_child("update", n->c, indent + 1);
            print_child("corpo", n->d, indent + 1);
            break;
        case NODE_DOWHILE:
            print_child("corpo", n->a, indent + 1);
            print_child("cond", n->b, indent + 1);
            break;
        case NODE_METHOD:
            print_child("params", n->a, indent + 1);
            print_child("corpo", n->b, indent + 1);
            break;
        case NODE_CALL:
            print_child("args", n->a, indent + 1);
            break;
        default:
            if (n->a) print_ast(n->a, indent + 1);
            if (n->b) print_ast(n->b, indent + 1);
            if (n->c) print_ast(n->c, indent + 1);
            if (n->d) print_ast(n->d, indent + 1);
            break;
        }
    }
}

/* ================= Liberacao ================= */

void free_ast(Node *n) {
    while (n) {
        Node *next = n->next;         /* guarda antes de liberar */
        free_ast(n->a);
        free_ast(n->b);
        free_ast(n->c);
        free_ast(n->d);
        free(n->name);                /* free(NULL) e seguro */
        free(n);
        n = next;
    }
}