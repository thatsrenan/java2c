/*
 * tests/ast_test.c
 *
 * Testes unitarios do modulo da AST (parser/ast.c). Nao depende do parser
 * nem do lexer: monta as arvores direto com os construtores new_* e
 * confere os campos, a ligacao das listas (append), os nomes legiveis
 * (type_name/op_name) e a saida de print_ast contra um texto esperado.
 *
 * O Makefile compila este harness com -fsanitize=address,undefined, entao
 * vazamentos, double free e acessos invalidos em free_ast tambem fazem o
 * teste falhar (o sanitizer encerra o programa com codigo != 0).
 *
 * Uso:  make test-ast
 * Codigo de saida: 0 se todos os testes passaram, 1 se algum falhou.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"

/* Mini framework */

static const char *G = "", *R = "", *B = "", *Z = "";

static const char *cur_test;
static int cur_failed;
static int total, passed;

static void report_failure(int line, const char *msg) {
    if (!cur_failed)
        printf("  %s[FALHA]%s %s\n", R, Z, cur_test);
    cur_failed = 1;
    printf("           linha %d: %s\n", line, msg);
}

#define CHECK(cond) do { \
    if (!(cond)) report_failure(__LINE__, "falhou: " #cond); \
} while (0)

#define CHECK_STR(got, exp) do { \
    const char *g_ = (got), *e_ = (exp); \
    if (!g_ || !e_ || strcmp(g_, e_) != 0) { \
        report_failure(__LINE__, #got " != " #exp); \
        printf("           esperado: \"%s\"\n", e_ ? e_ : "(NULL)"); \
        printf("           obtido:   \"%s\"\n", g_ ? g_ : "(NULL)"); \
    } \
} while (0)

static void run(const char *name, void (*fn)(void)) {
    cur_test = name;
    cur_failed = 0;
    total++;
    fn();
    if (!cur_failed) {
        passed++;
        printf("  %s[ OK ]%s  %s\n", G, Z, name);
    }
}

/* Strings de 'name' pertencem ao no (free_ast faz free), entao os testes
 * sempre passam copias alocadas. */
static char *s(const char *txt) { return strdup(txt); }

/* Roda print_ast redirecionando o stdout para um arquivo temporario e
 * devolve o texto impresso (o chamador libera). */
static char *capture_print(Node *n, int indent) {
    fflush(stdout);
    FILE *tmp = tmpfile();
    if (!tmp) { perror("tmpfile"); exit(2); }
    int saved = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);

    print_ast(n, indent);
    fflush(stdout);

    dup2(saved, STDOUT_FILENO);
    close(saved);

    long len = lseek(fileno(tmp), 0, SEEK_END);
    lseek(fileno(tmp), 0, SEEK_SET);
    char *buf = malloc(len + 1);
    ssize_t got = read(fileno(tmp), buf, len);
    buf[got > 0 ? got : 0] = '\0';
    fclose(tmp);
    return buf;
}

static int list_length(Node *n) {
    int k = 0;
    for (; n; n = n->next) k++;
    return k;
}

/* Construcao basica */

static void test_new_node_zera_campos(void) {
    Node *n = new_node(NODE_BLOCK, 42);
    CHECK(n->kind == NODE_BLOCK);
    CHECK(n->line == 42);
    CHECK(n->type == TYPE_UNKNOWN);
    CHECK(n->decl_type == TYPE_UNKNOWN);
    CHECK(n->is_global == 0);
    CHECK(n->name == NULL);
    CHECK(n->a == NULL && n->b == NULL && n->c == NULL && n->d == NULL);
    CHECK(n->next == NULL);
    free_ast(n);
}

static void test_append(void) {
    Node *x = new_int(1, 1), *y = new_int(2, 1), *z = new_int(3, 1);

    CHECK(append(NULL, NULL) == NULL);
    CHECK(append(NULL, x) == x);              /* lista vazia: n vira a lista */
    CHECK(append(x, NULL) == x);              /* nada a acrescentar */

    Node *list = append(append(x, y), z);
    CHECK(list == x);                          /* a cabeca nao muda */
    CHECK(list_length(list) == 3);
    CHECK(x->next == y && y->next == z && z->next == NULL);

    /* acrescentar uma cadeia traz todos os elementos dela */
    Node *p = new_int(4, 1);
    append(p, new_int(5, 1));
    append(list, p);
    CHECK(list_length(list) == 5);
    CHECK(z->next == p && p->next->val.i == 5);

    free_ast(list);
}

/* Construtores de expressao */

static void test_literais(void) {
    Node *i = new_int(123, 3);
    CHECK(i->kind == NODE_INT && i->val.i == 123 && i->line == 3);

    Node *r = new_real(2.5, 4);
    CHECK(r->kind == NODE_FLOAT && r->val.d == 2.5);

    Node *c = new_char(s("\\n"), 5);
    CHECK(c->kind == NODE_CHAR);
    CHECK_STR(c->name, "\\n");

    Node *st = new_str(s("ola"), 6);
    CHECK(st->kind == NODE_STR);
    CHECK(st->type == TYPE_STRING);           /* unico no que ja nasce tipado */
    CHECK_STR(st->name, "ola");

    Node *bt = new_bool(1, 7), *bf = new_bool(0, 7), *bn = new_bool(5, 7);
    CHECK(bt->kind == NODE_BOOL && bt->val.i == 1);
    CHECK(bf->val.i == 0);
    CHECK(bn->val.i == 1);                     /* qualquer valor != 0 vira 1 */

    Node *id = new_ident(s("x"), 8);
    CHECK(id->kind == NODE_IDENTIFIER);
    CHECK_STR(id->name, "x");

    free_ast(i); free_ast(r); free_ast(c); free_ast(st);
    free_ast(bt); free_ast(bf); free_ast(bn); free_ast(id);
}

static void test_operadores(void) {
    Node *l = new_ident(s("a"), 1), *rr = new_int(2, 1);
    Node *bin = new_binop(OP_MULT, l, rr, 1);
    CHECK(bin->kind == NODE_BINOP && bin->op == OP_MULT);
    CHECK(bin->a == l && bin->b == rr && bin->c == NULL);

    Node *opnd = new_bool(1, 2);
    Node *un = new_unary(OP_NOT, opnd, 2);
    CHECK(un->kind == NODE_UNARY && un->op == OP_NOT && un->a == opnd);

    Node *val = new_ident(s("y"), 3);
    Node *cast = new_cast(TYPE_INT, val, 3);
    CHECK(cast->kind == NODE_CAST && cast->decl_type == TYPE_INT && cast->a == val);
    CHECK(cast->type == TYPE_UNKNOWN);         /* o tipo da expressao e do semantico */

    Node *args = append(new_int(1, 4), new_ident(s("z"), 4));
    Node *call = new_call(s("f"), args, 4);
    CHECK(call->kind == NODE_CALL && call->a == args);
    CHECK_STR(call->name, "f");
    CHECK(list_length(call->a) == 2);

    Node *call0 = new_call(s("g"), NULL, 5);
    CHECK(call0->a == NULL);

    free_ast(bin); free_ast(un); free_ast(cast); free_ast(call); free_ast(call0);
}

/* Construtores de statement / programa */

static void test_statements(void) {
    Node *e = new_int(1, 1);
    Node *as = new_assign(OP_ADD_ASSIGN, s("x"), e, 1);
    CHECK(as->kind == NODE_ASSIGN && as->op == OP_ADD_ASSIGN && as->a == e);
    CHECK_STR(as->name, "x");

    Node *vd = new_vardecl(TYPE_DOUBLE, s("d"), NULL, 2);
    CHECK(vd->kind == NODE_VARDECL && vd->decl_type == TYPE_DOUBLE);
    CHECK(vd->a == NULL && vd->is_global == 0);

    Node *init = new_int(0, 3);
    Node *fd = new_field(TYPE_INT, s("N"), init, 3);
    CHECK(fd->kind == NODE_VARDECL && fd->is_global == 1 && fd->a == init);

    Node *inc = new_incdec(OP_PREDEC, s("i"), 4);
    CHECK(inc->kind == NODE_INCDEC && inc->op == OP_PREDEC && inc->a == NULL);

    free_ast(as); free_ast(vd); free_ast(fd); free_ast(inc);
}

static void test_programa(void) {
    Node *params = append(new_param(TYPE_INT, s("a"), 1),
                          new_param(TYPE_CHAR, s("b"), 1));
    CHECK(params->kind == NODE_PARAM && params->decl_type == TYPE_INT);
    CHECK(params->next->decl_type == TYPE_CHAR);

    Node *body = new_node(NODE_BLOCK, 1);
    Node *m = new_method(TYPE_BOOLEAN, s("f"), params, body, 1);
    CHECK(m->kind == NODE_METHOD && m->decl_type == TYPE_BOOLEAN);
    CHECK(m->a == params && m->b == body);
    CHECK(list_length(m->a) == 2);

    Node *field = new_field(TYPE_INT, s("g"), NULL, 1);
    Node *cls = new_class(s("Main"), append(field, m), 1);
    CHECK(cls->kind == NODE_CLASS && cls->a == field);
    CHECK_STR(cls->name, "Main");
    CHECK(field->next == m);

    free_ast(cls);
}

/* ================= Nomes legiveis ================= */

static void test_type_name(void) {
    CHECK_STR(type_name(TYPE_UNKNOWN), "?");
    CHECK_STR(type_name(TYPE_INT), "int");
    CHECK_STR(type_name(TYPE_FLOAT), "float");
    CHECK_STR(type_name(TYPE_DOUBLE), "double");
    CHECK_STR(type_name(TYPE_BOOLEAN), "boolean");
    CHECK_STR(type_name(TYPE_CHAR), "char");
    CHECK_STR(type_name(TYPE_VOID), "void");
    CHECK_STR(type_name(TYPE_STRING), "String");
    CHECK_STR(type_name(TYPE_ERROR), "<erro>");
}

static void test_op_name(void) {
    static const struct { OpKind op; const char *txt; } tab[] = {
        { OP_ADD, "+" },   { OP_SUB, "-" },   { OP_MULT, "*" },
        { OP_DIV, "/" },   { OP_MOD, "%" },
        { OP_LT, "<" },    { OP_GT, ">" },    { OP_LE, "<=" },
        { OP_GE, ">=" },   { OP_EQ, "==" },   { OP_NE, "!=" },
        { OP_AND, "&&" },  { OP_OR, "||" },
        { OP_NOT, "!" },   { OP_NEG, "-(unario)" },
        { OP_ASSIGN, "=" },          { OP_ADD_ASSIGN, "+=" },
        { OP_SUB_ASSIGN, "-=" },     { OP_MULT_ASSIGN, "*=" },
        { OP_DIV_ASSIGN, "/=" },
        { OP_PREINC, "++(pre)" },    { OP_POSTINC, "(pos)++" },
        { OP_PREDEC, "--(pre)" },    { OP_POSTDEC, "(pos)--" },
    };
    for (size_t k = 0; k < sizeof tab / sizeof tab[0]; k++)
        CHECK_STR(op_name(tab[k].op), tab[k].txt);
}

/* ================= print_ast ================= */

/*
 * Equivalente a:
 *
 *   class Main {
 *       static int N = 10;
 *       static int soma(int a, int b) { return a + b; }
 *       public static void main(String[] args) {
 *           for (int i = 0; i < N; i++) {
 *               if (i % 2 == 0) { System.out.println(soma(i, 1)); }
 *               else { continue; }
 *           }
 *       }
 *   }
 */
static Node *build_programa_exemplo(void) {
    Node *field = new_field(TYPE_INT, s("N"), new_int(10, 2), 2);

    Node *ret = new_node(NODE_RETURN, 3);
    ret->a = new_binop(OP_ADD, new_ident(s("a"), 3), new_ident(s("b"), 3), 3);
    Node *soma_body = new_node(NODE_BLOCK, 3);
    soma_body->a = ret;
    Node *soma = new_method(TYPE_INT, s("soma"),
                            append(new_param(TYPE_INT, s("a"), 3),
                                   new_param(TYPE_INT, s("b"), 3)),
                            soma_body, 3);

    Node *print = new_node(NODE_PRINT, 6);
    print->a = new_call(s("soma"), append(new_ident(s("i"), 6), new_int(1, 6)), 6);
    Node *then_blk = new_node(NODE_BLOCK, 6);
    then_blk->a = print;
    Node *else_blk = new_node(NODE_BLOCK, 7);
    else_blk->a = new_node(NODE_CONTINUE, 7);

    Node *iff = new_node(NODE_IF, 6);
    iff->a = new_binop(OP_EQ,
                       new_binop(OP_MOD, new_ident(s("i"), 6), new_int(2, 6), 6),
                       new_int(0, 6), 6);
    iff->b = then_blk;
    iff->c = else_blk;

    Node *for_body = new_node(NODE_BLOCK, 5);
    for_body->a = iff;

    Node *fr = new_node(NODE_FOR, 5);
    fr->a = new_vardecl(TYPE_INT, s("i"), new_int(0, 5), 5);
    fr->b = new_binop(OP_LT, new_ident(s("i"), 5), new_ident(s("N"), 5), 5);
    fr->c = new_incdec(OP_POSTINC, s("i"), 5);
    fr->d = for_body;

    Node *main_body = new_node(NODE_BLOCK, 4);
    main_body->a = fr;
    Node *main_m = new_method(TYPE_VOID, s("main"), NULL, main_body, 4);

    return new_class(s("Main"), append(append(field, soma), main_m), 1);
}

static void test_print_programa(void) {
    const char *esperado =
        "CLASS Main\n"
        "  FIELD(static) int N\n"
        "    INT 10\n"
        "  METHOD int soma\n"
        "    params:\n"
        "      PARAM int a\n"
        "      PARAM int b\n"
        "    corpo:\n"
        "      BLOCK\n"
        "        RETURN\n"
        "          BINOP +\n"
        "            ID a\n"
        "            ID b\n"
        "  METHOD void main\n"
        "    params:\n"
        "      (vazio)\n"
        "    corpo:\n"
        "      BLOCK\n"
        "        FOR\n"
        "          init:\n"
        "            VARDECL int i\n"
        "              INT 0\n"
        "          cond:\n"
        "            BINOP <\n"
        "              ID i\n"
        "              ID N\n"
        "          update:\n"
        "            INCDEC i (pos)++\n"
        "          corpo:\n"
        "            BLOCK\n"
        "              IF\n"
        "                cond:\n"
        "                  BINOP ==\n"
        "                    BINOP %\n"
        "                      ID i\n"
        "                      INT 2\n"
        "                    INT 0\n"
        "                then:\n"
        "                  BLOCK\n"
        "                    PRINT\n"
        "                      CALL soma\n"
        "                        args:\n"
        "                          ID i\n"
        "                          INT 1\n"
        "                else:\n"
        "                  BLOCK\n"
        "                    CONTINUE\n";

    Node *prog = build_programa_exemplo();
    char *got = capture_print(prog, 0);
    if (strcmp(got, esperado) != 0) {
        report_failure(__LINE__, "saida de print_ast diferente da esperada");
        printf("----- esperado -----\n%s----- obtido -----\n%s--------------------\n",
               esperado, got);
    }
    free(got);
    free_ast(prog);
}

static void test_print_literais_e_tipos(void) {
    /* if (!b) x = (double) -1; else if (c == 'a') y = 2.5; */
    Node *as1 = new_assign(OP_ASSIGN, s("x"),
                           new_cast(TYPE_DOUBLE, new_unary(OP_NEG, new_int(1, 1), 1), 1), 1);
    Node *as2 = new_assign(OP_ASSIGN, s("y"), new_real(2.5, 2), 2);

    Node *inner = new_node(NODE_IF, 2);
    inner->a = new_binop(OP_EQ, new_ident(s("c"), 2), new_char(s("a"), 2), 2);
    inner->b = as2;

    Node *outer = new_node(NODE_IF, 1);
    outer->a = new_unary(OP_NOT, new_ident(s("b"), 1), 1);
    outer->b = as1;
    outer->c = inner;                          /* else if */

    /* tipo preenchido (como o semantico fara) aparece no cabecalho */
    outer->a->type = TYPE_BOOLEAN;

    const char *esperado =
        "IF\n"
        "  cond:\n"
        "    UNARY !  : boolean\n"
        "      ID b\n"
        "  then:\n"
        "    ASSIGN x =\n"
        "      CAST (double)\n"
        "        UNARY -(unario)\n"
        "          INT 1\n"
        "  else:\n"
        "    IF\n"
        "      cond:\n"
        "        BINOP ==\n"
        "          ID c\n"
        "          CHAR 'a'\n"
        "      then:\n"
        "        ASSIGN y =\n"
        "          REAL 2.5\n";

    char *got = capture_print(outer, 0);
    if (strcmp(got, esperado) != 0) {
        report_failure(__LINE__, "saida de print_ast diferente da esperada");
        printf("----- esperado -----\n%s----- obtido -----\n%s--------------------\n",
               esperado, got);
    }
    free(got);
    free_ast(outer);
}

static void test_print_lacos_e_vazios(void) {
    /* while (true) { break; }  do { } while (false);  for (;;) ;  return; */
    Node *w = new_node(NODE_WHILE, 1);
    w->a = new_bool(1, 1);
    w->b = new_node(NODE_BLOCK, 1);
    w->b->a = new_node(NODE_BREAK, 1);

    Node *dw = new_node(NODE_DOWHILE, 2);
    dw->a = new_node(NODE_BLOCK, 2);
    dw->b = new_bool(0, 2);

    Node *fr = new_node(NODE_FOR, 3);           /* todos os filhos NULL */

    Node *ret = new_node(NODE_RETURN, 4);

    Node *print = new_node(NODE_PRINT, 5);
    print->a = new_str(s("fim"), 5);            /* STR nao mostra ": String" */

    Node *list = append(append(append(append(w, dw), fr), ret), print);

    const char *esperado =
        "WHILE\n"
        "  cond:\n"
        "    BOOL true\n"
        "  corpo:\n"
        "    BLOCK\n"
        "      BREAK\n"
        "DO-WHILE\n"
        "  corpo:\n"
        "    BLOCK\n"
        "  cond:\n"
        "    BOOL false\n"
        "FOR\n"
        "  init:\n"
        "    (vazio)\n"
        "  cond:\n"
        "    (vazio)\n"
        "  update:\n"
        "    (vazio)\n"
        "  corpo:\n"
        "    (vazio)\n"
        "RETURN\n"
        "PRINT\n"
        "  STR \"fim\"\n";

    char *got = capture_print(list, 0);
    if (strcmp(got, esperado) != 0) {
        report_failure(__LINE__, "saida de print_ast diferente da esperada");
        printf("----- esperado -----\n%s----- obtido -----\n%s--------------------\n",
               esperado, got);
    }
    free(got);

    /* indentacao inicial desloca a arvore inteira */
    Node *one = new_int(7, 1);
    got = capture_print(one, 2);
    CHECK_STR(got, "    INT 7\n");
    free(got);
    free_ast(one);

    free_ast(list);
}

/* ================= free_ast ================= */

static void test_free_ast(void) {
    free_ast(NULL);                            /* nao pode quebrar */

    /* arvore grande + lista longa: com o sanitizer, qualquer vazamento ou
     * double free aqui derruba o executavel */
    Node *prog = build_programa_exemplo();
    free_ast(prog);

    Node *list = NULL;
    for (int k = 0; k < 1000; k++)
        list = append(list, new_ident(s("v"), k));
    CHECK(list_length(list) == 1000);
    free_ast(list);
}

/* ================= main ================= */

int main(void) {
    if (isatty(STDOUT_FILENO)) {
        G = "\033[32m"; R = "\033[31m"; B = "\033[1m"; Z = "\033[0m";
    }

    printf("%sTestes unitarios da AST (parser/ast.c)%s\n\n", B, Z);

    run("new_node zera os campos",               test_new_node_zera_campos);
    run("append liga listas",                    test_append);
    run("construtores de literais",              test_literais);
    run("construtores de operadores/chamada",    test_operadores);
    run("construtores de statements",            test_statements);
    run("construtores de parametro/metodo/classe", test_programa);
    run("type_name",                             test_type_name);
    run("op_name",                               test_op_name);
    run("print_ast de um programa completo",     test_print_programa);
    run("print_ast de literais, cast e else-if", test_print_literais_e_tipos);
    run("print_ast de lacos e filhos vazios",    test_print_lacos_e_vazios);
    run("free_ast libera tudo",                  test_free_ast);

    printf("\n");
    int pct = 100 * passed / total;
    if (passed == total) {
        printf("%s%sResultado: %d/%d testes passaram (%d%%).%s\n", G, B, passed, total, pct, Z);
        return 0;
    }
    printf("%s%sResultado: %d/%d testes passaram (%d%%).%s\n", R, B, passed, total, pct, Z);
    return 1;
}
