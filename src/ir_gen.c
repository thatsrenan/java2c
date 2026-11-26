#include "ir_gen.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "symbol_table.h"
#include "const_eval.h"
#include "ir_opt.h"

/*
 * ir_gen.c
 *
 * Percorre a AST e emite código de três endereços (TAC).
 *
 * Estratégia geral:
 * - gen_expr: traduz uma expressão e devolve o operando que contém seu
 *   valor (constante, variável ou temporário). Operadores lógicos
 *   (&&, ||) são tratados com rótulos para preservar curto-circuito.
 * - gen_stmt / gen_block: emitem o fluxo de controle e efeitos.
 * - Laços mantêm os rótulos de destino de 'break' e 'continue' em uma
 *   pequena pilha de contexto.
 * - Tipos: o parser não preenche n->type das expressões, então o
 *   gerador mantém o próprio ambiente (nome -> tipo) de globais,
 *   parâmetros e locais, e deduz o tipo de cada expressão (expr_type).
 * - Nomes: cada variável recebe um nome na IR único dentro da função
 *   (ver unique_name), de modo que a IR não tem ocultação de nomes.
 */

/* ===================== Contexto de geração ===================== */

typedef struct LoopCtx {
    char            *break_label;    /* destino de break */
    char            *continue_label; /* destino de continue */
    struct LoopCtx  *prev;
} LoopCtx;

/* Associação nome Java -> (nome na IR, tipo). 'name' é emprestado da
 * AST e 'ir_name' da própria IR (IrGlobal/IrParam/IrLocal); nenhum dos
 * dois é liberado aqui. */
typedef struct VarBind {
    const char     *name;
    const char     *ir_name;
    IrType          type;
    struct VarBind *next;
} VarBind;

/* Nome de função Java -> nome na IR (difere só se colidir com palavra
 * reservada de C ou com o padrão dos temporários). */
typedef struct FuncName {
    const char      *name;
    char            *ir_name;
    struct FuncName *next;
} FuncName;

/* Global cujo inicializador não é constante: vira código no início do
 * main (ou de __init_globals, se o programa não tiver main). */
typedef struct PendingInit {
    Node               *decl;
    struct PendingInit *next;
} PendingInit;

typedef struct {
    IrProgram   *prog;
    IrFunc      *func;       /* função atual recebendo instruções */
    LoopCtx     *loop;       /* topo da pilha de laços (para break/continue) */
    VarBind     *locals;     /* pilha de parâmetros/locais visíveis */
    VarBind     *globals;    /* campos static */
    FuncName    *funcs;      /* nomes de todas as funções do programa */
    PendingInit *pending;    /* inicializações de globais a emitir */
    int          inits_done; /* 1 depois que 'pending' foi emitido */
} GenCtx;

static void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (!p) {
        fprintf(stderr, "ERRO: sem memoria no gerador de IR.\n");
        exit(1);
    }
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *p = strdup(s);
    if (!p) {
        fprintf(stderr, "ERRO: sem memoria no gerador de IR.\n");
        exit(1);
    }
    return p;
}

/* ===================== Ambiente de tipos ===================== */

static void bind_push(VarBind **list, const char *name, const char *ir_name,
                      IrType type) {
    VarBind *b = xcalloc(1, sizeof *b);
    b->name = name;
    b->ir_name = ir_name;
    b->type = type;
    b->next = *list;
    *list = b;
}

/* Desempilha locais até voltar à marca 'mark' (fim de escopo). */
static void locals_restore(GenCtx *ctx, VarBind *mark) {
    while (ctx->locals && ctx->locals != mark) {
        VarBind *next = ctx->locals->next;
        free(ctx->locals);
        ctx->locals = next;
    }
}

static void bind_free_all(VarBind *b) {
    while (b) {
        VarBind *next = b->next;
        free(b);
        b = next;
    }
}

static VarBind *lookup_var(GenCtx *ctx, const char *name) {
    if (!name) return NULL;
    for (VarBind *b = ctx->locals; b; b = b->next)
        if (strcmp(b->name, name) == 0) return b;
    for (VarBind *b = ctx->globals; b; b = b->next)
        if (strcmp(b->name, name) == 0) return b;
    return NULL;
}

static IrType lookup_var_type(GenCtx *ctx, const char *name) {
    VarBind *b = lookup_var(ctx, name);
    return b ? b->type : TYPE_UNKNOWN;
}

/* Operando de variável já com o nome da IR. */
static IrOperand var_op(GenCtx *ctx, const char *name, IrType type) {
    VarBind *b = lookup_var(ctx, name);
    return ir_var(b ? b->ir_name : name, type);
}

static const char *func_ir_name(GenCtx *ctx, const char *name) {
    for (FuncName *fn = ctx->funcs; fn; fn = fn->next)
        if (strcmp(fn->name, name) == 0) return fn->ir_name;
    return name;
}

/* ===================== Nomes únicos ===================== */

/* Palavras reservadas de C (C11) que são identificadores válidos em Java,
 * e nomes usados pelo C gerado. */
static int is_c_reserved(const char *s) {
    static const char *words[] = {
        "auto", "extern", "inline", "register", "restrict", "signed",
        "sizeof", "struct", "typedef", "union", "unsigned", "volatile",
        "_Bool", "_Complex", "_Imaginary", "_Alignas", "_Alignof",
        "_Atomic", "_Generic", "_Noreturn", "_Static_assert",
        "_Thread_local", "printf", NULL
    };
    for (int i = 0; words[i]; i++)
        if (strcmp(s, words[i]) == 0) return 1;
    return 0;
}

/* Nomes no formato dos temporários (t0, t12, ...) ficam para a IR. */
static int looks_like_temp(const char *s) {
    if (s[0] != 't' || !s[1]) return 0;
    for (const char *p = s + 1; *p; p++)
        if (*p < '0' || *p > '9') return 0;
    return 1;
}

/* Um nome de variável está ocupado se colide com palavra reservada,
 * temporário, função, global ou outra variável da função atual. */
static int var_name_taken(GenCtx *ctx, const char *s) {
    if (is_c_reserved(s) || looks_like_temp(s)) return 1;
    for (FuncName *fn = ctx->funcs; fn; fn = fn->next)
        if (strcmp(fn->ir_name, s) == 0) return 1;
    for (IrGlobal *g = ctx->prog->globals; g; g = g->next)
        if (strcmp(g->name, s) == 0) return 1;
    if (ctx->func) {
        for (IrParam *p = ctx->func->params; p; p = p->next)
            if (strcmp(p->name, s) == 0) return 1;
        for (IrLocal *l = ctx->func->locals; l; l = l->next)
            if (strcmp(l->name, s) == 0) return 1;
    }
    return 0;
}

/* Nome livre a partir de 'base': base, base_1, base_2, ...
 * Retorna string nova (o chamador libera). */
static char *unique_name(GenCtx *ctx, const char *base,
                         int (*taken)(GenCtx *, const char *)) {
    if (!taken(ctx, base)) return xstrdup(base);
    size_t len = strlen(base) + 16;
    char *buf = xcalloc(len, 1);
    for (int i = 1;; i++) {
        snprintf(buf, len, "%s_%d", base, i);
        if (!taken(ctx, buf)) return buf;
    }
}

static int func_name_taken(GenCtx *ctx, const char *s) {
    if (is_c_reserved(s) || looks_like_temp(s)) return 1;
    for (FuncName *fn = ctx->funcs; fn; fn = fn->next)
        if (strcmp(fn->ir_name, s) == 0) return 1;
    return 0;
}

/* Converte DataType (tabela de símbolos) para Type (AST/IR). */
static IrType type_from_dt(DataType dt) {
    switch (dt) {
    case ST_TYPE_INT:     return TYPE_INT;
    case ST_TYPE_FLOAT:   return TYPE_FLOAT;
    case ST_TYPE_DOUBLE:  return TYPE_DOUBLE;
    case ST_TYPE_BOOLEAN: return TYPE_BOOLEAN;
    case ST_TYPE_CHAR:    return TYPE_CHAR;
    case ST_TYPE_VOID:    return TYPE_VOID;
    case ST_TYPE_STRING:  return TYPE_STRING;
    default:              return TYPE_UNKNOWN;
    }
}

/* Tipo de retorno de uma função, consultando a tabela de símbolos (ainda
 * viva durante a geração da IR). Sem isso, chamadas void ganhariam um
 * temporário de resultado indevidamente. */
static IrType lookup_func_return_type(const char *name) {
    if (!name) return TYPE_UNKNOWN;
    Symbol *s = st_lookup_global(name);
    if (!s || s->kind != SYM_FUNCTION) return TYPE_UNKNOWN;
    return type_from_dt(s->type);
}

static int is_real(IrType t) { return t == TYPE_FLOAT || t == TYPE_DOUBLE; }

/* Promoção numérica binária do Java: double > float > int (char vira int). */
static IrType promote(IrType a, IrType b) {
    if (a == TYPE_DOUBLE || b == TYPE_DOUBLE) return TYPE_DOUBLE;
    if (a == TYPE_FLOAT  || b == TYPE_FLOAT)  return TYPE_FLOAT;
    return TYPE_INT;
}

static int is_relational(OpKind op) {
    return op == OP_LT || op == OP_GT || op == OP_LE || op == OP_GE ||
           op == OP_EQ || op == OP_NE;
}

/* Tipo estático de uma expressão. */
static IrType expr_type(GenCtx *ctx, Node *n) {
    if (!n) return TYPE_UNKNOWN;
    switch (n->kind) {
    case NODE_INT:   return TYPE_INT;
    case NODE_FLOAT: return n->decl_type == TYPE_FLOAT ? TYPE_FLOAT : TYPE_DOUBLE;
    case NODE_CHAR:  return TYPE_CHAR;
    case NODE_BOOL:  return TYPE_BOOLEAN;
    case NODE_STR:   return TYPE_STRING;
    case NODE_IDENTIFIER: {
        IrType t = lookup_var_type(ctx, n->name);
        if (t != TYPE_UNKNOWN) return t;
        return n->type != TYPE_UNKNOWN ? n->type : TYPE_INT;
    }
    case NODE_UNARY:
        if (n->op == OP_NOT) return TYPE_BOOLEAN;
        return promote(expr_type(ctx, n->a), TYPE_INT);
    case NODE_BINOP:
        if (n->op == OP_AND || n->op == OP_OR || is_relational(n->op))
            return TYPE_BOOLEAN;
        return promote(expr_type(ctx, n->a), expr_type(ctx, n->b));
    case NODE_CAST:
        return n->decl_type;
    case NODE_CALL: {
        IrType t = lookup_func_return_type(n->name);
        return t != TYPE_UNKNOWN ? t : TYPE_INT;
    }
    default:
        return n->type != TYPE_UNKNOWN ? n->type : TYPE_INT;
    }
}

/* ===================== Constantes ===================== */

static IrOperand const_operand(ConstVal c) {
    if (is_real(c.type)) return ir_const_real(c.d, c.type);
    return ir_const_int(c.i, c.type);
}

/* Avalia 'n' em tempo de compilação (ver const_eval.c). */
static int fold_const(Node *n, IrOperand *out) {
    ConstVal c;
    if (!const_eval(n, &c)) return 0;
    *out = const_operand(c);
    return 1;
}

/* Valor padrão de um tipo (campo static sem inicializador, seção 3.1). */
static IrOperand default_value(IrType t) {
    if (is_real(t)) return ir_const_real(0.0, t);
    return ir_const_int(0, t);
}

/* ===================== Utilitários de emissão ===================== */

static IrOperand new_temp_op(GenCtx *ctx, IrType type) {
    return ir_temp(ir_new_temp(ctx->func), type);
}

static IrInstr *emit_label(GenCtx *ctx, const char *name) {
    IrInstr *ins = ir_emit(ctx->func, IR_LABEL);
    ins->res = ir_label(name);
    return ins;
}

static void emit_goto(GenCtx *ctx, const char *label) {
    IrInstr *ins = ir_emit(ctx->func, IR_GOTO);
    ins->res = ir_label(label);
}

static void emit_if_false(GenCtx *ctx, IrOperand cond, const char *label) {
    IrInstr *ins = ir_emit(ctx->func, IR_IF_FALSE);
    ins->arg1 = cond;
    ins->res = ir_label(label);
}

static void emit_assign(GenCtx *ctx, IrOperand dst, IrOperand src, IrType type) {
    IrInstr *ins = ir_emit(ctx->func, IR_ASSIGN);
    ins->type = type;
    ins->res = dst;
    ins->arg1 = src;
}

/* ===================== Geração de expressões ===================== */

static IrOperand gen_expr(GenCtx *ctx, Node *n);
static IrOperand gen_expr_to(GenCtx *ctx, Node *n, IrType t);
static void gen_stmt(GenCtx *ctx, Node *n);

/* Gera IR para obter o endereço/valor de uma condição booleana como
 * operando (0/1). Reaproveita gen_expr, que já trata &&/|| com
 * curto-circuito produzindo um temporário booleano quando necessário. */
static IrOperand gen_cond(GenCtx *ctx, Node *cond) {
    return gen_expr(ctx, cond);
}

/* Curto-circuito: a && b
 *   t = a
 *   ifFalse t goto Lfalse
 *   t = b
 *   goto Lend
 * Lfalse:
 *   t = false
 * Lend:   (t contém o resultado)
 */
static IrOperand gen_logical_and(GenCtx *ctx, Node *n) {
    IrOperand t = new_temp_op(ctx, TYPE_BOOLEAN);
    char *lfalse = ir_new_label(ctx->prog, "Land_false");
    char *lend   = ir_new_label(ctx->prog, "Land_end");

    IrOperand left = gen_expr(ctx, n->a);
    emit_assign(ctx, ir_temp(t.id, t.type), left, TYPE_BOOLEAN);
    emit_if_false(ctx, ir_temp(t.id, t.type), lfalse);

    IrOperand right = gen_expr(ctx, n->b);
    emit_assign(ctx, ir_temp(t.id, t.type), right, TYPE_BOOLEAN);
    emit_goto(ctx, lend);

    emit_label(ctx, lfalse);
    emit_assign(ctx, ir_temp(t.id, t.type), ir_const_int(0, TYPE_BOOLEAN), TYPE_BOOLEAN);
    emit_label(ctx, lend);

    free(lfalse);
    free(lend);
    return t;
}

/* Curto-circuito: a || b
 *   t = a
 *   if t goto Ltrue
 *   t = b
 *   goto Lend
 * Ltrue:
 *   t = true
 * Lend:
 */
static IrOperand gen_logical_or(GenCtx *ctx, Node *n) {
    IrOperand t = new_temp_op(ctx, TYPE_BOOLEAN);
    char *ltrue = ir_new_label(ctx->prog, "Lor_true");
    char *lend  = ir_new_label(ctx->prog, "Lor_end");

    IrOperand left = gen_expr(ctx, n->a);
    emit_assign(ctx, ir_temp(t.id, t.type), left, TYPE_BOOLEAN);

    IrInstr *jt = ir_emit(ctx->func, IR_IF_GOTO);
    jt->arg1 = ir_temp(t.id, t.type);
    jt->res = ir_label(ltrue);

    IrOperand right = gen_expr(ctx, n->b);
    emit_assign(ctx, ir_temp(t.id, t.type), right, TYPE_BOOLEAN);
    emit_goto(ctx, lend);

    emit_label(ctx, ltrue);
    emit_assign(ctx, ir_temp(t.id, t.type), ir_const_int(1, TYPE_BOOLEAN), TYPE_BOOLEAN);
    emit_label(ctx, lend);

    free(ltrue);
    free(lend);
    return t;
}

/* 1 se a expressão contém alguma chamada (que pode alterar globais). */
static int has_call(Node *n) {
    if (!n) return 0;
    if (n->kind == NODE_CALL) return 1;
    if (n->kind == NODE_BINOP || n->kind == NODE_UNARY || n->kind == NODE_CAST)
        return has_call(n->a) || has_call(n->b);
    return 0;
}

/* Chamada: avalia TODOS os argumentos primeiro e só então emite os
 * 'param' em sequência, seguidos do 'call'. Assim chamadas aninhadas
 * (f(1, g(2))) nunca intercalam seus 'param' com os da chamada externa. */
static IrOperand gen_call(GenCtx *ctx, Node *n) {
    int nargs = 0;
    for (Node *arg = n->a; arg; arg = arg->next) nargs++;

    IrOperand *vals = nargs ? xcalloc((size_t)nargs, sizeof *vals) : NULL;
    Symbol *fs = st_lookup_global(n->name);
    int i = 0;
    for (Node *arg = n->a; arg; arg = arg->next, i++) {
        IrType pt = TYPE_UNKNOWN;
        if (fs && fs->kind == SYM_FUNCTION && i < fs->param_count)
            pt = type_from_dt(fs->param_types[i]);
        vals[i] = gen_expr_to(ctx, arg, pt);
        /* Java avalia da esquerda para a direita: se um argumento
         * posterior faz uma chamada (que pode alterar uma global), o
         * valor de uma variável já avaliada é copiado antes. */
        if (vals[i].kind == IR_OP_VAR) {
            int later_call = 0;
            for (Node *nx = arg->next; nx; nx = nx->next)
                if (has_call(nx)) { later_call = 1; break; }
            if (later_call) {
                IrOperand t = new_temp_op(ctx, vals[i].type);
                emit_assign(ctx, t, vals[i], vals[i].type);
                vals[i] = ir_temp(t.id, t.type);
            }
        }
    }
    for (i = 0; i < nargs; i++) {
        IrInstr *p = ir_emit(ctx->func, IR_PARAM);
        p->arg1 = vals[i];
    }
    free(vals);

    IrOperand dst = ir_empty();
    IrType rt = expr_type(ctx, n);
    if (rt != TYPE_VOID) dst = new_temp_op(ctx, rt);
    IrInstr *c = ir_emit(ctx->func, IR_CALL);
    c->type = rt;
    c->res = dst;
    c->arg1 = ir_func_ref(func_ir_name(ctx, n->name));
    c->arg2 = ir_const_int(nargs, TYPE_INT);
    if (dst.kind == IR_OP_TEMP) return ir_temp(dst.id, dst.type);
    return dst; /* vazio se void; chamador como stmt ignora */
}

/* Constante usada num destino de tipo 't' (atribuição, parâmetro,
 * return): já sai convertida, para a IR não ter conversão implícita de
 * constantes (ex.: 'double d = 2' vira 'd = 2.0'). */
static IrOperand coerce_const(IrOperand op, IrType t) {
    if (op.kind != IR_OP_CONST || op.type == t || t == TYPE_UNKNOWN ||
        t == TYPE_VOID || t == TYPE_BOOLEAN || op.type == TYPE_BOOLEAN)
        return op;
    ConstVal c = { op.type, op.value.i, op.value.d };
    return const_operand(const_convert(c, t));
}

/* Gera uma expressão cujo valor vai para um destino do tipo 't'. A única
 * conversão de estreitamento implícita do Java (constante int que cabe
 * em char, ex.: char c = 'a' + 1) é resolvida aqui, em tempo de
 * compilação; as demais conversões implícitas são alargamentos. */
static IrOperand gen_expr_to(GenCtx *ctx, Node *n, IrType t) {
    if (t == TYPE_CHAR && expr_type(ctx, n) == TYPE_INT) {
        ConstVal c;
        if (const_eval(n, &c)) return const_operand(const_convert(c, TYPE_CHAR));
    }
    return coerce_const(gen_expr(ctx, n), t);
}

static IrOperand gen_expr(GenCtx *ctx, Node *n) {
    if (!n) return ir_empty();

    switch (n->kind) {
    case NODE_INT:
    case NODE_FLOAT:
    case NODE_CHAR:
    case NODE_BOOL: {
        IrOperand c;
        fold_const(n, &c);
        return c;
    }

    case NODE_IDENTIFIER:
        /* Variável/local/global/parâmetro: em TAC referenciamos pelo nome. */
        return var_op(ctx, n->name, expr_type(ctx, n));

    case NODE_UNARY: {
        IrOperand val = gen_expr(ctx, n->a);
        IrType t = expr_type(ctx, n);
        IrOperand dst = new_temp_op(ctx, t);
        IrInstr *ins = ir_emit(ctx->func, IR_UNARY);
        ins->type = t;
        ins->unop = n->op;
        ins->res = dst;
        ins->arg1 = val;
        return ir_temp(dst.id, t);
    }

    case NODE_BINOP:
        /* && e || exigem curto-circuito. */
        if (n->op == OP_AND) return gen_logical_and(ctx, n);
        if (n->op == OP_OR)  return gen_logical_or(ctx, n);
        {
            IrOperand l = gen_expr(ctx, n->a);
            IrOperand r = gen_expr(ctx, n->b);
            IrType t = expr_type(ctx, n);
            IrOperand dst = new_temp_op(ctx, t);
            IrInstr *ins = ir_emit(ctx->func, IR_BINOP);
            ins->type = t;
            ins->binop = n->op;
            ins->res = dst;
            ins->arg1 = l;
            ins->arg2 = r;
            return ir_temp(dst.id, t);
        }

    case NODE_CAST: {
        IrOperand val = gen_expr(ctx, n->a);
        IrOperand dst = new_temp_op(ctx, n->decl_type);
        IrInstr *ins = ir_emit(ctx->func, IR_CAST);
        ins->type = n->decl_type;
        ins->cast_type = n->decl_type;
        ins->res = dst;
        ins->arg1 = val;
        return ir_temp(dst.id, n->decl_type);
    }

    case NODE_CALL:
        return gen_call(ctx, n);

    default:
        /* Nó de expressão não esperado: retorna vazio. */
        return ir_empty();
    }
}

/* ===================== Geração de statements ===================== */

static void gen_block_stmts(GenCtx *ctx, Node *block) {
    if (!block) return;
    /* Um bloco abre escopo: locais declarados nele somem ao final. */
    VarBind *mark = ctx->locals;
    Node *s = (block->kind == NODE_BLOCK) ? block->a : block;
    for (; s; s = s->next) gen_stmt(ctx, s);
    locals_restore(ctx, mark);
}

static void push_loop(GenCtx *ctx, char *break_label, char *cont_label) {
    LoopCtx *lc = xcalloc(1, sizeof *lc);
    lc->break_label = break_label;
    lc->continue_label = cont_label;
    lc->prev = ctx->loop;
    ctx->loop = lc;
}

static void pop_loop(GenCtx *ctx) {
    if (!ctx->loop) return;
    LoopCtx *prev = ctx->loop->prev;
    free(ctx->loop->break_label);
    free(ctx->loop->continue_label);
    free(ctx->loop);
    ctx->loop = prev;
}

/* Atribuição com o tipo do DESTINO (variável), não o do lado direito. */
static void gen_assign_stmt(GenCtx *ctx, Node *n) {
    /* n->name = variável alvo, n->op = tipo de atribuição, n->a = expr */
    IrType vt = lookup_var_type(ctx, n->name);
    if (vt == TYPE_UNKNOWN) vt = expr_type(ctx, n->a);
    IrOperand rhs = (n->op == OP_ASSIGN) ? gen_expr_to(ctx, n->a, vt)
                                         : gen_expr(ctx, n->a);

    if (n->op == OP_ASSIGN) {
        emit_assign(ctx, var_op(ctx, n->name, vt), rhs, vt);
        return;
    }

    /* Atribuição composta (Java): x op= e  =>  x = (T)(x op e)
     *   t = x op e        (no tipo promovido)
     *   [t' = (T) t]      (só se o tipo promovido difere de T)
     *   x = t | t'                                           */
    IrBinOp bop;
    switch (n->op) {
    case OP_SUB_ASSIGN:  bop = OP_SUB; break;
    case OP_MULT_ASSIGN: bop = OP_MULT; break;
    case OP_DIV_ASSIGN:  bop = OP_DIV; break;
    default:             bop = OP_ADD; break;
    }
    IrType bt = promote(vt, expr_type(ctx, n->a));
    IrOperand tmp = new_temp_op(ctx, bt);
    IrInstr *bin = ir_emit(ctx->func, IR_BINOP);
    bin->type = bt;
    bin->binop = bop;
    bin->res = tmp;
    bin->arg1 = var_op(ctx, n->name, vt);
    bin->arg2 = rhs;

    IrOperand result = ir_temp(tmp.id, bt);
    if (bt != vt) {
        IrOperand cast = new_temp_op(ctx, vt);
        IrInstr *ci = ir_emit(ctx->func, IR_CAST);
        ci->type = vt;
        ci->cast_type = vt;
        ci->res = cast;
        ci->arg1 = result;
        result = ir_temp(cast.id, vt);
    }
    emit_assign(ctx, var_op(ctx, n->name, vt), result, vt);
}

/* ++/-- (pré/pós) como statement: x = x +/- 1, no tipo da variável. Para
 * char o resultado é int e precisa voltar a char com cast. */
static void gen_incdec_stmt(GenCtx *ctx, Node *n) {
    IrType vt = lookup_var_type(ctx, n->name);
    if (vt == TYPE_UNKNOWN) vt = TYPE_INT;
    IrType bt = promote(vt, TYPE_INT);

    IrOperand tmp = new_temp_op(ctx, bt);
    IrInstr *bin = ir_emit(ctx->func, IR_BINOP);
    bin->type = bt;
    bin->binop = (n->op == OP_PREINC || n->op == OP_POSTINC) ? OP_ADD : OP_SUB;
    bin->res = tmp;
    bin->arg1 = var_op(ctx, n->name, vt);
    bin->arg2 = ir_const_int(1, TYPE_INT);

    IrOperand result = ir_temp(tmp.id, bt);
    if (bt != vt) {
        IrOperand cast = new_temp_op(ctx, vt);
        IrInstr *ci = ir_emit(ctx->func, IR_CAST);
        ci->type = vt;
        ci->cast_type = vt;
        ci->res = cast;
        ci->arg1 = result;
        result = ir_temp(cast.id, vt);
    }
    emit_assign(ctx, var_op(ctx, n->name, vt), result, vt);
}

static void gen_if(GenCtx *ctx, Node *n) {
    /* O rótulo de else só é criado quando existe else. */
    char *lelse = n->c ? ir_new_label(ctx->prog, "Lelse") : NULL;
    char *lend  = ir_new_label(ctx->prog, "Lendif");

    IrOperand cond = gen_cond(ctx, n->a);
    emit_if_false(ctx, cond, n->c ? lelse : lend);

    /* then */
    gen_block_stmts(ctx, n->b);

    if (n->c) {
        emit_goto(ctx, lend);
        emit_label(ctx, lelse);
        if (n->c->kind == NODE_IF) {
            /* else-if encadeado: gera como if (sem criar bloco extra) */
            gen_if(ctx, n->c);
        } else {
            gen_block_stmts(ctx, n->c);
        }
    }

    emit_label(ctx, lend);
    free(lelse);
    free(lend);
}

static void gen_while(GenCtx *ctx, Node *n) {
    char *lcond = ir_new_label(ctx->prog, "Lwhile_cond");
    char *lend  = ir_new_label(ctx->prog, "Lwhile_end");

    emit_label(ctx, lcond);
    IrOperand cond = gen_cond(ctx, n->a);
    emit_if_false(ctx, cond, lend);

    push_loop(ctx, xstrdup(lend), xstrdup(lcond));
    gen_block_stmts(ctx, n->b);
    pop_loop(ctx);

    emit_goto(ctx, lcond);
    emit_label(ctx, lend);

    free(lcond);
    free(lend);
}

static void gen_do_while(GenCtx *ctx, Node *n) {
    char *lbody = ir_new_label(ctx->prog, "Ldo_body");
    char *lcond = ir_new_label(ctx->prog, "Ldo_cond");
    char *lend  = ir_new_label(ctx->prog, "Ldo_end");

    emit_label(ctx, lbody);

    /* continue deve saltar para a avaliação da condição */
    push_loop(ctx, xstrdup(lend), xstrdup(lcond));
    gen_block_stmts(ctx, n->a);
    pop_loop(ctx);

    emit_label(ctx, lcond);
    IrOperand cond = gen_cond(ctx, n->b);
    {
        IrInstr *jt = ir_emit(ctx->func, IR_IF_GOTO);
        jt->arg1 = cond;
        jt->res = ir_label(lbody);
    }
    emit_label(ctx, lend);

    free(lbody);
    free(lcond);
    free(lend);
}

static void gen_for(GenCtx *ctx, Node *n) {
    /* Estrutura:
     *   init
     * Lcond: if (!cond) goto Lend   (se houver cond)
     *   body
     * Lcont: update                 (continue salta aqui)
     *   goto Lcond
     * Lend:
     */
    char *lcond = ir_new_label(ctx->prog, "Lfor_cond");
    char *lcont = ir_new_label(ctx->prog, "Lfor_cont");
    char *lend  = ir_new_label(ctx->prog, "Lfor_end");

    /* A variável declarada no init só existe dentro do for. */
    VarBind *mark = ctx->locals;

    /* init (executa uma vez). Pode ser VARDECL, ASSIGN ou NULL. */
    if (n->a) gen_stmt(ctx, n->a);

    emit_label(ctx, lcond);
    if (n->b) {
        IrOperand cond = gen_cond(ctx, n->b);
        emit_if_false(ctx, cond, lend);
    }

    push_loop(ctx, xstrdup(lend), xstrdup(lcont));
    gen_block_stmts(ctx, n->d);
    pop_loop(ctx);

    emit_label(ctx, lcont);
    if (n->c) {
        /* update: ASSIGN ou INCDEC */
        gen_stmt(ctx, n->c);
    }
    emit_goto(ctx, lcond);
    emit_label(ctx, lend);

    locals_restore(ctx, mark);
    free(lcond);
    free(lcont);
    free(lend);
}

static void gen_stmt(GenCtx *ctx, Node *n) {
    if (!n) return;

    switch (n->kind) {
    case NODE_BLOCK:
        gen_block_stmts(ctx, n);
        break;

    case NODE_VARDECL: {
        /* Declaração local: entra na lista de locais da função com um
         * nome único e, se há init, gera atribuição. O init é gerado
         * antes de registrar o nome, como em Java. */
        IrOperand rhs = n->a ? gen_expr_to(ctx, n->a, n->decl_type) : ir_empty();
        char *irn = unique_name(ctx, n->name, var_name_taken);
        ir_func_add_local(ctx->func, irn, n->decl_type);
        free(irn);
        IrLocal *l = ctx->func->locals;
        while (l->next) l = l->next;
        bind_push(&ctx->locals, n->name, l->name, n->decl_type);
        if (n->a) emit_assign(ctx, ir_var(l->name, n->decl_type), rhs, n->decl_type);
        break;
    }

    case NODE_ASSIGN:
        gen_assign_stmt(ctx, n);
        break;

    case NODE_INCDEC:
        gen_incdec_stmt(ctx, n);
        break;

    case NODE_PRINT: {
        /* Gera o valor antes de emitir o print: assim qualquer instrução
         * necessária para calcular a expressão aparece na ordem correta. */
        int is_string = n->a && n->a->kind == NODE_STR;
        IrOperand value = is_string ? ir_empty() : gen_expr(ctx, n->a);
        IrInstr *p = ir_emit(ctx->func, IR_PRINT);
        if (is_string) {
            p->print_string = xstrdup(n->a->name ? n->a->name : "");
            p->arg1 = ir_empty();
        } else {
            p->print_string = NULL;
            p->arg1 = value;
        }
        break;
    }

    case NODE_IF:
        gen_if(ctx, n);
        break;

    case NODE_WHILE:
        gen_while(ctx, n);
        break;

    case NODE_DOWHILE:
        gen_do_while(ctx, n);
        break;

    case NODE_FOR:
        gen_for(ctx, n);
        break;

    case NODE_BREAK:
        if (ctx->loop && ctx->loop->break_label) {
            emit_goto(ctx, ctx->loop->break_label);
        }
        /* se não houver laço (programa inválido), nada a emitir */
        break;

    case NODE_CONTINUE:
        if (ctx->loop && ctx->loop->continue_label) {
            emit_goto(ctx, ctx->loop->continue_label);
        }
        break;

    case NODE_RETURN: {
        /* Igual ao print: calcula a expressão primeiro e só depois emite
         * o return, para não inverter a ordem das instruções. */
        IrOperand value = n->a ? gen_expr_to(ctx, n->a, ctx->func->ret_type) : ir_empty();
        IrInstr *r = ir_emit(ctx->func, IR_RETURN);
        r->arg1 = value;
        break;
    }

    case NODE_CALL:
        /* Chamada usada como statement: gera a chamada e descarta resultado. */
        (void)gen_expr(ctx, n);
        break;

    default:
        /* Tipos de nó não tratados como statement (ex.: METHOD/PARAM/CLASS)
         * são ignorados aqui; o driver trata a estrutura do programa. */
        break;
    }
}

/* ===================== Estrutura do programa ===================== */

/* Registra os nomes das funções (antes de tudo: variáveis não podem
 * colidir com eles na IR). */
static void collect_funcs(GenCtx *ctx, Node *member) {
    FuncName **tail = &ctx->funcs;
    for (Node *m = member; m; m = m->next) {
        if (m->kind != NODE_METHOD || !m->name) continue;
        FuncName *fn = xcalloc(1, sizeof *fn);
        fn->name = m->name;
        fn->ir_name = unique_name(ctx, m->name, func_name_taken);
        *tail = fn;
        tail = &fn->next;
    }
}

/* Registra os campos static. Todo global recebe valor inicial na IR:
 *  - inicializador constante (literais, -1, 2 + 3, (double) 1, ...):
 *    o valor, já convertido para o tipo declarado;
 *  - sem inicializador: o valor padrão do tipo (especificação, 3.1);
 *  - inicializador não constante: o valor padrão, e a atribuição vira
 *    código no início do main. A partir do primeiro desses, os globais
 *    seguintes também são atribuídos em código, na ordem do fonte, como
 *    no Java (um inicializador pode chamar função que lê um global
 *    declarado depois, que ainda deve valer o padrão nesse momento). */
static void collect_globals(GenCtx *ctx, Node *member) {
    PendingInit **tail = &ctx->pending;
    int in_order = 0;
    for (Node *m = member; m; m = m->next) {
        if (m->kind != NODE_VARDECL || !m->is_global) continue;

        IrOperand init = default_value(m->decl_type);
        if (m->a) {
            IrOperand c;
            if (!in_order && fold_const(m->a, &c)) {
                ConstVal cv = { c.type, c.value.i, c.value.d };
                init = const_operand(const_convert(cv, m->decl_type));
            } else {
                in_order = 1;
                PendingInit *pi = xcalloc(1, sizeof *pi);
                pi->decl = m;
                *tail = pi;
                tail = &pi->next;
            }
        }
        char *irn = unique_name(ctx, m->name, var_name_taken);
        ir_program_add_global(ctx->prog, irn, m->decl_type, 1, init);
        free(irn);
        IrGlobal *g = ctx->prog->globals;
        while (g->next) g = g->next;
        bind_push(&ctx->globals, m->name, g->name, m->decl_type);
    }
}

/* Emite as inicializações pendentes de globais na função atual. */
static void emit_pending_inits(GenCtx *ctx) {
    if (ctx->inits_done) return;
    ctx->inits_done = 1;
    for (PendingInit *pi = ctx->pending; pi; pi = pi->next) {
        Node *m = pi->decl;
        IrOperand rhs = gen_expr_to(ctx, m->a, m->decl_type);
        emit_assign(ctx, var_op(ctx, m->name, m->decl_type), rhs, m->decl_type);
    }
}

static void free_pending(GenCtx *ctx) {
    PendingInit *pi = ctx->pending;
    while (pi) {
        PendingInit *next = pi->next;
        free(pi);
        pi = next;
    }
    ctx->pending = NULL;
}

static int is_main_name(const char *name) {
    return name && strcmp(name, "main") == 0;
}

static void gen_function(GenCtx *ctx, Node *method) {
    IrFunc *f = ir_program_add_func(ctx->prog,
                                    method->name ? func_ir_name(ctx, method->name) : "anon",
                                    method->decl_type);
    IrFunc *prev_func = ctx->func;
    VarBind *mark = ctx->locals;
    ctx->func = f;

    /* parâmetros */
    for (Node *p = method->a; p; p = p->next) {
        if (p->kind == NODE_PARAM) {
            char *irn = unique_name(ctx, p->name, var_name_taken);
            ir_func_add_param(f, irn, p->decl_type);
            free(irn);
            IrParam *ip = f->params;
            while (ip->next) ip = ip->next;
            bind_push(&ctx->locals, p->name, ip->name, p->decl_type);
        }
    }

    if (is_main_name(method->name)) emit_pending_inits(ctx);

    /* corpo: NODE_BLOCK em b */
    if (method->b) gen_block_stmts(ctx, method->b);

    locals_restore(ctx, mark);
    ctx->func = prev_func;
}

static void gen_implicit_main(GenCtx *ctx, Node *block) {
    /* Bloco de statements soltos (programa sem classe): vira função main. */
    IrFunc *f = ir_program_add_func(ctx->prog, "main", TYPE_VOID);
    IrFunc *prev_func = ctx->func;
    ctx->func = f;
    emit_pending_inits(ctx);
    gen_block_stmts(ctx, block);
    ctx->func = prev_func;
}

IrProgram *ir_generate(Node *root) {
    IrProgram *prog = ir_program_new();
    if (!root) {
        ir_optimize(prog);
        return prog;
    }

    GenCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.prog = prog;

    /* Raiz esperada: NODE_CLASS (nome real ou implícita com name == NULL). */
    Node *classNode = root;
    if (classNode->kind != NODE_CLASS) {
        /* fallback: trata como bloco solto */
        IrFunc *f = ir_program_add_func(prog, "main", TYPE_VOID);
        ctx.func = f;
        gen_stmt(&ctx, classNode);
        locals_restore(&ctx, NULL);
        ir_optimize(prog);
        return prog;
    }

    /* 1) Coleta nomes de funções e globais (campos static). */
    collect_funcs(&ctx, classNode->a);
    collect_globals(&ctx, classNode->a);

    /* 2) Gera métodos e bloco solto (se houver). */
    for (Node *m = classNode->a; m; m = m->next) {
        if (m->kind == NODE_METHOD) {
            gen_function(&ctx, m);
        } else if (m->kind == NODE_BLOCK) {
            /* Bloco de statements soltos acumulados pelo parser. */
            gen_implicit_main(&ctx, m);
        }
    }

    /* 3) Sem main para receber as inicializações não constantes de
     *    globais: elas vão para uma função própria, para não se perderem. */
    if (ctx.pending && !ctx.inits_done) {
        ctx.func = ir_program_add_func(prog, "__init_globals", TYPE_VOID);
        emit_pending_inits(&ctx);
        ctx.func = NULL;
    }

    free_pending(&ctx);
    bind_free_all(ctx.globals);
    for (FuncName *fn = ctx.funcs; fn;) {
        FuncName *next = fn->next;
        free(fn->ir_name);
        free(fn);
        fn = next;
    }
    locals_restore(&ctx, NULL);
    ir_optimize(prog);
    return prog;
}
