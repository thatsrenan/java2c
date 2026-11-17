#include "semantic_flow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "const_eval.h"

/*
 * Implementação
 *
 * Cada variável local de um método recebe um índice. O conjunto de locais
 * definitivamente atribuídas é um vetor de bytes (DaSet). Um conjunto com
 * 'all' = 1 representa um ponto inalcançável: nele, por vacuidade, toda
 * variável conta como atribuída (é o "V is definitely assigned after any
 * statement that cannot complete normally" do JLS).
 *
 * Como no subconjunto atribuição e ++/-- são apenas comandos, expressões
 * nunca atribuem; elas só leem variáveis. Ainda assim && / || / ! e
 * constantes seguem as regras de "atribuída quando verdadeiro/falso" para
 * casos como 'if (false && x > 0)' e 'while (true)'.
 */

typedef struct {
    int            all;   /* 1 = inalcançável (contém todas) */
    unsigned char *bits;  /* bits[i] = 1 se a local i está atribuída */
} DaSet;

/* Local visível no escopo atual. Parâmetros e globais não entram: estão
 * sempre atribuídos. 'name' é emprestado da AST. */
typedef struct Binding {
    const char     *name;
    int             id;        /* índice da local; -1 = parâmetro (sempre atribuído) */
    int             reported;  /* já houve erro de "não inicializada" para ela */
    struct Binding *next;
} Binding;

/* Laço em análise: acumula os estados nos 'break' e 'continue'. */
typedef struct Loop {
    DaSet        brk;       /* interseção dos estados nos break */
    DaSet        cont;      /* interseção dos estados nos continue */
    int          has_break; /* existe break alcançável */
    int          has_cont;  /* existe continue alcançável */
    struct Loop *prev;
} Loop;

typedef struct {
    int      nvars;     /* capacidade dos DaSet do método atual */
    int      next_id;   /* próximo índice de local */
    Binding *scope;     /* pilha de nomes visíveis */
    Loop    *loop;      /* laço mais interno */
    int      errors;
} Flow;

static void flow_error(Flow *f, int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "ERRO SEMANTICO na linha %d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    f->errors++;
}

static void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n ? n : 1, sz);
    if (!p) {
        fprintf(stderr, "ERRO: sem memoria na analise de fluxo.\n");
        exit(1);
    }
    return p;
}

/* ===================== Conjuntos ===================== */

static DaSet da_new(Flow *f, int all) {
    DaSet s;
    s.all = all;
    s.bits = xcalloc((size_t)f->nvars, 1);
    return s;
}

static DaSet da_copy(Flow *f, DaSet s) {
    DaSet c = da_new(f, s.all);
    memcpy(c.bits, s.bits, (size_t)f->nvars);
    return c;
}

static void da_free(DaSet *s) {
    free(s->bits);
    s->bits = NULL;
}

/* *dst = *dst ∩ src (o inalcançável é o elemento neutro). */
static void da_meet(Flow *f, DaSet *dst, DaSet src) {
    if (src.all) return;
    if (dst->all) {
        memcpy(dst->bits, src.bits, (size_t)f->nvars);
        dst->all = 0;
        return;
    }
    for (int i = 0; i < f->nvars; i++) dst->bits[i] &= src.bits[i];
}

static int da_has(DaSet s, int id) {
    return s.all || s.bits[id];
}

/* ===================== Escopo ===================== */

static Binding *lookup(Flow *f, const char *name) {
    for (Binding *b = f->scope; b; b = b->next)
        if (strcmp(b->name, name) == 0) return b;
    return NULL; /* global (ou erro já reportado pelo parser) */
}

static void bind(Flow *f, const char *name, int id) {
    Binding *b = xcalloc(1, sizeof *b);
    b->name = name;
    b->id = id;
    b->next = f->scope;
    f->scope = b;
}

static void scope_restore(Flow *f, Binding *mark) {
    while (f->scope && f->scope != mark) {
        Binding *next = f->scope->next;
        free(f->scope);
        f->scope = next;
    }
}

/* Leitura de variável: precisa estar definitivamente atribuída. */
static void use_var(Flow *f, const char *name, int line, DaSet s) {
    Binding *b = lookup(f, name);
    if (!b || b->id < 0 || da_has(s, b->id) || b->reported) return;
    b->reported = 1;
    flow_error(f, line, "a variavel '%s' pode nao ter sido inicializada", name);
}

/* ===================== Expressões ===================== */

static void check_expr(Flow *f, Node *n, DaSet s);

/* Estados depois de uma condição: quando verdadeira (*t) e falsa (*fl). */
static void check_cond(Flow *f, Node *n, DaSet s, DaSet *t, DaSet *fl) {
    int v;
    if (n && n->kind == NODE_BINOP && (n->op == OP_AND || n->op == OP_OR)) {
        DaSet at, af, bt, bf;
        check_cond(f, n->a, s, &at, &af);
        if (n->op == OP_AND) {
            check_cond(f, n->b, at, &bt, &bf);
            *t = bt;
            da_meet(f, &af, bf);
            *fl = af;
            da_free(&at);
            da_free(&bf);
        } else {
            check_cond(f, n->b, af, &bt, &bf);
            da_meet(f, &at, bt);
            *t = at;
            *fl = bf;
            da_free(&af);
            da_free(&bt);
        }
        return;
    }
    if (n && n->kind == NODE_UNARY && n->op == OP_NOT) {
        check_cond(f, n->a, s, fl, t);
        return;
    }
    check_expr(f, n, s);
    *t = da_copy(f, s);
    *fl = da_copy(f, s);
    if (const_bool(n, &v)) {
        /* constante: o ramo impossível é inalcançável */
        if (v) fl->all = 1; else t->all = 1;
    }
}

static void check_expr(Flow *f, Node *n, DaSet s) {
    if (!n) return;
    switch (n->kind) {
    case NODE_IDENTIFIER:
        use_var(f, n->name, n->line, s);
        break;
    case NODE_BINOP:
        if (n->op == OP_AND || n->op == OP_OR) {
            DaSet t, fl;
            check_cond(f, n, s, &t, &fl);
            da_free(&t);
            da_free(&fl);
        } else {
            check_expr(f, n->a, s);
            check_expr(f, n->b, s);
        }
        break;
    case NODE_UNARY:
    case NODE_CAST:
        check_expr(f, n->a, s);
        break;
    case NODE_CALL:
        for (Node *a = n->a; a; a = a->next) check_expr(f, a, s);
        break;
    default:
        break;
    }
}

/* ===================== Comandos ===================== */

/* Resultado de um comando: estado de atribuição depois dele e se ele
 * pode terminar normalmente (alcançabilidade, JLS 14.22). As duas coisas
 * seguem regras diferentes: em 'if (false) { return; }' o comando
 * termina normalmente, mas dentro do ramo tudo conta como atribuído. */
typedef struct {
    DaSet da;
    int   ccn;  /* can complete normally */
} Res;

static Res res(DaSet da, int ccn) {
    Res r = { da, ccn };
    return r;
}

static Res check_stmt(Flow *f, Node *n, DaSet s);

/* Lista de comandos (corpo de bloco). Um comando depois de outro que não
 * termina normalmente é inalcançável: erro (reportado uma vez por lista). */
static Res check_list(Flow *f, Node *first, DaSet s) {
    Res cur = res(da_copy(f, s), 1);
    int reported = 0;
    for (Node *st = first; st; st = st->next) {
        if (!cur.ccn && !reported) {
            flow_error(f, st->line, "codigo inalcancavel");
            reported = 1;
        }
        Res next = check_stmt(f, st, cur.da);
        if (reported) next.ccn = 0; /* continua analisando, sem cascata */
        da_free(&cur.da);
        cur = next;
    }
    return cur;
}

static Res check_block(Flow *f, Node *n, DaSet s) {
    Binding *mark = f->scope;
    Res out = (n && n->kind == NODE_BLOCK) ? check_list(f, n->a, s)
                                           : check_stmt(f, n, s);
    scope_restore(f, mark);
    return out;
}

static void push_loop(Flow *f, Loop *lp) {
    lp->brk = da_new(f, 1);
    lp->cont = da_new(f, 1);
    lp->has_break = lp->has_cont = 0;
    lp->prev = f->loop;
    f->loop = lp;
}

static void pop_loop(Flow *f, Loop *lp) {
    f->loop = lp->prev;
    da_free(&lp->brk);
    da_free(&lp->cont);
}

/* while (c) S  e  for (; c; u) S */
static Res check_loop(Flow *f, Node *cond, Node *update, Node *body, DaSet s) {
    DaSet t, fl;
    int cv = 1, is_const = 1;   /* sem condição = true constante */
    if (cond) {
        is_const = const_bool(cond, &cv);
        check_cond(f, cond, s, &t, &fl);
    } else {
        t = da_copy(f, s);
        fl = da_new(f, 1);
    }
    if (is_const && !cv) {
        /* condição constante false: o corpo é inalcançável (JLS 14.22) */
        flow_error(f, body ? body->line : cond->line, "codigo inalcancavel");
    }

    Loop lp;
    push_loop(f, &lp);
    Res body_r = check_block(f, body, t);
    if (update) {
        /* o update roda depois do corpo ou de um continue */
        da_meet(f, &body_r.da, lp.cont);
        if (body_r.ccn || lp.has_cont) {
            Res u = check_stmt(f, update, body_r.da);
            da_free(&u.da);
        }
    }
    /* sai do laço quando a condição é falsa ou por um break */
    DaSet out = fl;
    da_meet(f, &out, lp.brk);
    int ccn = !(is_const && cv) || lp.has_break;
    pop_loop(f, &lp);
    da_free(&t);
    da_free(&body_r.da);
    return res(out, ccn);
}

static Res check_stmt(Flow *f, Node *n, DaSet s) {
    if (!n) return res(da_copy(f, s), 1);

    switch (n->kind) {
    case NODE_BLOCK:
        return check_block(f, n, s);

    case NODE_VARDECL: {
        check_expr(f, n->a, s);  /* o inicializador ainda não vê a própria variável */
        DaSet out = da_copy(f, s);
        int id = f->next_id++;
        bind(f, n->name, id);
        if (n->a) out.bits[id] = 1;
        return res(out, 1);
    }

    case NODE_ASSIGN: {
        check_expr(f, n->a, s);
        if (n->op != OP_ASSIGN) use_var(f, n->name, n->line, s); /* x op= e lê x */
        DaSet out = da_copy(f, s);
        Binding *b = lookup(f, n->name);
        if (b && b->id >= 0) out.bits[b->id] = 1;
        return res(out, 1);
    }

    case NODE_INCDEC:
        use_var(f, n->name, n->line, s);
        return res(da_copy(f, s), 1);

    case NODE_PRINT:
        check_expr(f, n->a, s);
        return res(da_copy(f, s), 1);

    case NODE_CALL:
        check_expr(f, n, s);
        return res(da_copy(f, s), 1);

    case NODE_RETURN:
        check_expr(f, n->a, s);
        return res(da_new(f, 1), 0);

    case NODE_BREAK:
        if (f->loop) {
            da_meet(f, &f->loop->brk, s);
            f->loop->has_break = 1;
        }
        return res(da_new(f, 1), 0);

    case NODE_CONTINUE:
        if (f->loop) {
            da_meet(f, &f->loop->cont, s);
            f->loop->has_cont = 1;
        }
        return res(da_new(f, 1), 0);

    case NODE_IF: {
        /* Alcançabilidade: os ramos são sempre alcançáveis (if (false) é
         * permitido em Java). Atribuição definida: a constante conta. */
        DaSet t, fl;
        check_cond(f, n->a, s, &t, &fl);
        Res then_r = check_block(f, n->b, t);
        Res out;
        if (n->c) {
            Res else_r = (n->c->kind == NODE_IF) ? check_stmt(f, n->c, fl)
                                                 : check_block(f, n->c, fl);
            out = res(then_r.da, then_r.ccn || else_r.ccn);
            da_meet(f, &out.da, else_r.da);
            da_free(&else_r.da);
        } else {
            out = res(then_r.da, 1);
            da_meet(f, &out.da, fl);
        }
        da_free(&t);
        da_free(&fl);
        return out;
    }

    case NODE_WHILE:
        return check_loop(f, n->a, NULL, n->b, s);

    case NODE_FOR: {
        Binding *mark = f->scope;
        Res init = check_stmt(f, n->a, s);
        Res out = check_loop(f, n->b, n->c, n->d, init.da);
        da_free(&init.da);
        scope_restore(f, mark);
        return out;
    }

    case NODE_DOWHILE: {
        Loop lp;
        push_loop(f, &lp);
        Res body_r = check_block(f, n->a, s);
        da_meet(f, &body_r.da, lp.cont); /* continue vai para a condição */
        int cond_reach = body_r.ccn || lp.has_cont;
        int cv = 0, is_const = const_bool(n->b, &cv);
        DaSet t, fl;
        check_cond(f, n->b, body_r.da, &t, &fl);
        DaSet out = fl;
        da_meet(f, &out, lp.brk);
        int ccn = (cond_reach && !(is_const && cv)) || lp.has_break;
        pop_loop(f, &lp);
        da_free(&t);
        da_free(&body_r.da);
        return res(out, ccn);
    }

    default:
        return res(da_copy(f, s), 1);
    }
}

/* ===================== Métodos e programa ===================== */

/* Conta as declarações de locais (dimensiona os DaSet do método). */
static int count_vardecls(Node *n) {
    int c = 0;
    for (; n; n = n->next) {
        if (n->kind == NODE_VARDECL) c++;
        c += count_vardecls(n->a) + count_vardecls(n->b) +
             count_vardecls(n->c) + count_vardecls(n->d);
    }
    return c;
}

/* Analisa um corpo (método ou bloco solto). Retorna se ele pode terminar
 * normalmente, ou seja, "cair" no fim sem return. */
static int check_body(Flow *f, Node *params, Node *body) {
    f->nvars = count_vardecls(body);
    f->next_id = 0;
    for (Node *p = params; p; p = p->next)
        if (p->kind == NODE_PARAM) bind(f, p->name, -1);
    DaSet start = da_new(f, 0);
    Res r = check_block(f, body, start);
    da_free(&start);
    da_free(&r.da);
    scope_restore(f, NULL);
    return r.ccn;
}

int semantic_flow_check(Node *root) {
    Flow f;
    memset(&f, 0, sizeof f);
    if (!root) return 0;

    if (root->kind != NODE_CLASS) {
        check_body(&f, NULL, root);
        return f.errors;
    }

    for (Node *m = root->a; m; m = m->next) {
        if (m->kind == NODE_METHOD) {
            int falls = check_body(&f, m->a, m->b);
            if (falls && m->decl_type != TYPE_VOID) {
                flow_error(&f, m->line,
                           "o metodo '%s' deve retornar um valor do tipo %s em todos os caminhos",
                           m->name, type_name(m->decl_type));
            }
        } else if (m->kind == NODE_BLOCK) {
            check_body(&f, NULL, m);
        }
    }
    return f.errors;
}
