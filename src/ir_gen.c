#include "ir_gen.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "symbol_table.h"

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
 */

/* ===================== Contexto de geração ===================== */

typedef struct LoopCtx {
    char            *break_label;    /* destino de break */
    char            *continue_label; /* destino de continue */
    struct LoopCtx  *prev;
} LoopCtx;

typedef struct {
    IrProgram *prog;
    IrFunc    *func;     /* função atual recebendo instruções */
    LoopCtx   *loop;     /* topo da pilha de laços (para break/continue) */
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

/* Obtém o tipo "efetivo" de um nó da AST. Quando o nó traz um tipo
 * declarado específico (ex.: literal float com sufixo, cast, vardecl),
 * ele é considerado; caso contrário usa-se n->type. */
static IrType node_type(Node *n) {
    if (!n) return TYPE_UNKNOWN;
    switch (n->kind) {
    case NODE_FLOAT:
        return (n->decl_type == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_DOUBLE;
    case NODE_CAST:
    case NODE_VARDECL:
    case NODE_PARAM:
    case NODE_METHOD:
        return n->decl_type;
    default:
        if (n->type != TYPE_UNKNOWN) return n->type;
        return n->decl_type != TYPE_UNKNOWN ? n->decl_type : TYPE_UNKNOWN;
    }
}

/* Converte o tipo de um nó para o tipo de constante/temporário usado em
 * aritmética simples quando não há informação melhor. */
static IrType expr_ir_type(Node *n) {
    IrType t = node_type(n);
    if (t == TYPE_UNKNOWN) return TYPE_INT; /* fallback conservador */
    return t;
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

/* ===================== Utilitários de emissão ===================== */

static IrOperand new_temp_op(GenCtx *ctx, IrType type) {
    int id = ir_new_temp(ctx->prog);
    if (ctx->func && id + 1 > ctx->func->temp_count) {
        ctx->func->temp_count = id + 1;
    }
    return ir_temp(id, type);
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

static IrOperand gen_expr(GenCtx *ctx, Node *n) {
    if (!n) return ir_empty();

    switch (n->kind) {
    case NODE_INT:
        return ir_const_int(n->val.i, TYPE_INT);

    case NODE_FLOAT:
        return ir_const_real(n->val.d,
                             n->decl_type == TYPE_FLOAT ? TYPE_FLOAT : TYPE_DOUBLE);

    case NODE_CHAR: {
        /* n->name guarda o texto cru do literal (ex.: "a", "\n").
         * Armazenamos um valor inteiro simples: para um caractere comum
         * usamos seu código; escapes básicos são tratados de forma direta. */
        long v = 0;
        if (n->name) {
            if (n->name[0] == '\\' && n->name[1] != '\0') {
                switch (n->name[1]) {
                case 'n': v = '\n'; break;
                case 't': v = '\t'; break;
                case 'r': v = '\r'; break;
                case '0': v = '\0'; break;
                case '\\': v = '\\'; break;
                case '\'': v = '\''; break;
                case '"': v = '"'; break;
                default: v = (unsigned char)n->name[1]; break;
                }
            } else {
                v = (unsigned char)n->name[0];
            }
        }
        return ir_const_int(v, TYPE_CHAR);
    }

    case NODE_BOOL:
        return ir_const_int(n->val.i ? 1 : 0, TYPE_BOOLEAN);

    case NODE_IDENTIFIER:
        /* Variável/local/global/parâmetro: em TAC referenciamos pelo nome.
         * O tipo vem da AST (preenchido/consultável) ou é desconhecido. */
        return ir_var(n->name, expr_ir_type(n));

    case NODE_UNARY: {
        IrOperand val = gen_expr(ctx, n->a);
        IrType t = (n->op == OP_NOT) ? TYPE_BOOLEAN : expr_ir_type(n->a);
        IrOperand dst = new_temp_op(ctx, t);
        IrInstr *ins = ir_emit(ctx->func, IR_UNARY);
        ins->type = t;
        ins->unop = n->op;
        ins->res = dst;
        ins->arg1 = val;
        return dst;
    }

    case NODE_BINOP:
        /* && e || exigem curto-circuito. */
        if (n->op == OP_AND) return gen_logical_and(ctx, n);
        if (n->op == OP_OR)  return gen_logical_or(ctx, n);
        {
            IrOperand l = gen_expr(ctx, n->a);
            IrOperand r = gen_expr(ctx, n->b);
            IrType t;
            switch (n->op) {
            case OP_LT: case OP_GT: case OP_LE: case OP_GE:
            case OP_EQ: case OP_NE:
                t = TYPE_BOOLEAN;
                break;
            default:
                t = expr_ir_type(n);
                break;
            }
            IrOperand dst = new_temp_op(ctx, t);
            IrInstr *ins = ir_emit(ctx->func, IR_BINOP);
            ins->type = t;
            ins->binop = n->op;
            ins->res = dst;
            ins->arg1 = l;
            ins->arg2 = r;
            return dst;
        }

    case NODE_CAST: {
        IrOperand val = gen_expr(ctx, n->a);
        IrOperand dst = new_temp_op(ctx, n->decl_type);
        IrInstr *ins = ir_emit(ctx->func, IR_CAST);
        ins->type = n->decl_type;
        ins->cast_type = n->decl_type;
        ins->res = dst;
        ins->arg1 = val;
        return dst;
    }

    case NODE_CALL: {
        /* Emite 'param' para cada argumento, na ordem, depois 'call'. */
        int nargs = 0;
        for (Node *arg = n->a; arg; arg = arg->next) {
            IrOperand av = gen_expr(ctx, arg);
            IrInstr *p = ir_emit(ctx->func, IR_PARAM);
            p->arg1 = av;
            nargs++;
        }
        IrOperand dst = ir_empty();
        IrType rt = lookup_func_return_type(n->name);
        if (rt == TYPE_UNKNOWN) rt = expr_ir_type(n);
        if (rt != TYPE_VOID) {
            dst = new_temp_op(ctx, rt);
        }
        IrInstr *c = ir_emit(ctx->func, IR_CALL);
        c->type = rt;
        c->res = dst;
        c->arg1 = ir_func_ref(n->name);
        c->arg2 = ir_const_int(nargs, TYPE_INT);
        return dst; /* vazio se void; chamador como stmt ignora */
    }

    default:
        /* Nó de expressão não esperado: retorna vazio. */
        return ir_empty();
    }
}

/* ===================== Geração de statements ===================== */

static void gen_block_stmts(GenCtx *ctx, Node *block) {
    if (!block) return;
    /* NODE_BLOCK: filhos encadeados em a; percorre também next por segurança. */
    Node *s = (block->kind == NODE_BLOCK) ? block->a : block;
    for (; s; s = s->next) gen_stmt(ctx, s);
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

static void gen_assign_stmt(GenCtx *ctx, Node *n) {
    /* n->name = variável alvo, n->op = tipo de atribuição, n->a = expr */
    IrOperand rhs = gen_expr(ctx, n->a);
    IrType t = expr_ir_type(n->a);
    IrOperand dst = ir_var(n->name, t);

    if (n->op == OP_ASSIGN) {
        emit_assign(ctx, dst, rhs, t);
        return;
    }

    /* Atribuição composta: x <op>= e  =>  tmp = x <op> e; x = tmp */
    IrBinOp bop = OP_ADD;
    switch (n->op) {
    case OP_ADD_ASSIGN:  bop = OP_ADD; break;
    case OP_SUB_ASSIGN:  bop = OP_SUB; break;
    case OP_MULT_ASSIGN: bop = OP_MULT; break;
    case OP_DIV_ASSIGN:  bop = OP_DIV; break;
    default: bop = OP_ADD; break;
    }
    IrOperand cur = ir_var(n->name, t);
    IrOperand tmp = new_temp_op(ctx, t);
    IrInstr *bin = ir_emit(ctx->func, IR_BINOP);
    bin->type = t;
    bin->binop = bop;
    bin->res = tmp;
    bin->arg1 = cur;
    bin->arg2 = rhs;
    emit_assign(ctx, ir_var(n->name, t), ir_temp(tmp.id, t), t);
}

static void gen_incdec_stmt(GenCtx *ctx, Node *n) {
    /* ++/-- (pré/pós) como statement: efeito é sempre x = x +/- 1. */
    IrType t = TYPE_INT; /* tipos numéricos suportados; int é base comum */
    IrOperand one = ir_const_int(1, TYPE_INT);
    IrOperand cur = ir_var(n->name, t);
    IrOperand tmp = new_temp_op(ctx, t);
    IrInstr *bin = ir_emit(ctx->func, IR_BINOP);
    bin->type = t;
    bin->binop = (n->op == OP_PREINC || n->op == OP_POSTINC) ? OP_ADD : OP_SUB;
    bin->res = tmp;
    bin->arg1 = cur;
    bin->arg2 = one;
    emit_assign(ctx, ir_var(n->name, t), ir_temp(tmp.id, t), t);
}

static void gen_if(GenCtx *ctx, Node *n) {
    char *lelse = ir_new_label(ctx->prog, "Lelse");
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
    } else {
        /* sem else: lelse não usado como alvo real, mas label alocado */
        /* caímos direto em lend */
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

    case NODE_VARDECL:
        /* Declaração local/global inicializada: se há init, gera atribuição.
         * (A "alocação" em si é conceitual; na geração de C vira declaração.) */
        if (n->a) {
            IrOperand rhs = gen_expr(ctx, n->a);
            IrType t = n->decl_type != TYPE_UNKNOWN ? n->decl_type : expr_ir_type(n->a);
            emit_assign(ctx, ir_var(n->name, t), rhs, t);
        }
        break;

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
        IrOperand value = n->a ? gen_expr(ctx, n->a) : ir_empty();
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

static void collect_globals(GenCtx *ctx, Node *member) {
    for (Node *m = member; m; m = m->next) {
        if (m->kind == NODE_VARDECL && m->is_global) {
            int has_init = 0;
            IrOperand init = ir_empty();
            /* Apenas inicializadores constantes simples viram 'init' direto;
             * inicializadores complexos seriam emitidos em código (fora do
             * escopo simples atual). */
            if (m->a) {
                if (m->a->kind == NODE_INT) {
                    init = ir_const_int(m->a->val.i, TYPE_INT); has_init = 1;
                } else if (m->a->kind == NODE_FLOAT) {
                    IrType ft = (m->a->decl_type == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_DOUBLE;
                    init = ir_const_real(m->a->val.d, ft); has_init = 1;
                } else if (m->a->kind == NODE_BOOL) {
                    init = ir_const_int(m->a->val.i ? 1 : 0, TYPE_BOOLEAN); has_init = 1;
                } else if (m->a->kind == NODE_CHAR) {
                    long v = 0;
                    if (m->a->name) v = (unsigned char)m->a->name[0];
                    init = ir_const_int(v, TYPE_CHAR); has_init = 1;
                }
            }
            ir_program_add_global(ctx->prog, m->name, m->decl_type, has_init, init);
        }
    }
}

static void gen_function(GenCtx *ctx, Node *method) {
    IrFunc *f = ir_program_add_func(ctx->prog, method->name ? method->name : "anon",
                                    method->decl_type);
    /* parâmetros */
    for (Node *p = method->a; p; p = p->next) {
        if (p->kind == NODE_PARAM) {
            ir_func_add_param(f, p->name, p->decl_type);
        }
    }

    IrFunc *prev_func = ctx->func;
    ctx->func = f;
    /* corpo: NODE_BLOCK em b */
    if (method->b) gen_block_stmts(ctx, method->b);
    ctx->func = prev_func;
}

static void gen_implicit_main(GenCtx *ctx, Node *block) {
    /* Bloco de statements soltos (programa sem classe): vira função main. */
    IrFunc *f = ir_program_add_func(ctx->prog, "main", TYPE_VOID);
    IrFunc *prev_func = ctx->func;
    ctx->func = f;
    gen_block_stmts(ctx, block);
    ctx->func = prev_func;
}

IrProgram *ir_generate(Node *root) {
    IrProgram *prog = ir_program_new();
    if (!root) return prog;

    GenCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.prog = prog;
    ctx.func = NULL;
    ctx.loop = NULL;

    /* Raiz esperada: NODE_CLASS (nome real ou implícita com name == NULL). */
    Node *classNode = root;
    if (classNode->kind != NODE_CLASS) {
        /* fallback: trata como bloco solto */
        IrFunc *f = ir_program_add_func(prog, "main", TYPE_VOID);
        ctx.func = f;
        gen_stmt(&ctx, classNode);
        ctx.func = NULL;
        return prog;
    }

    /* 1) Coleta globais (campos static). */
    collect_globals(&ctx, classNode->a);

    /* 2) Gera métodos e bloco solto (se houver). */
    for (Node *m = classNode->a; m; m = m->next) {
        if (m->kind == NODE_METHOD) {
            gen_function(&ctx, m);
        } else if (m->kind == NODE_BLOCK) {
            /* Bloco de statements soltos acumulados pelo parser. */
            gen_implicit_main(&ctx, m);
        } else if (m->kind == NODE_VARDECL && m->is_global) {
            /* Se o inicializador não for constante simples, emite atribuição
             * no início de um "main" implícito? Para manter simples, apenas
             * ignoramos aqui: inicializadores constantes já foram coletados.
             * (Casos complexos são raros no subconjunto.) */
            /* Se houver init não-constante, poderíamos gerar código em uma
             * função de init; deixamos como melhoria futura. */
        }
    }

    return prog;
}
