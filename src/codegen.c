#include "codegen.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * codegen.c
 *
 * Traduz a TAC (ver ir.h) para C legível. O subconjunto Java aceito pelo
 * front-end é procedural: não há objetos, arrays ou String como tipo.
 * Assim, as classes Java já foram removidas na IR e o gerador precisa apenas
 * de globais, funções e fluxo de controle com rótulos.
 *
 * Convenções do modo bruto (codegen_emit):
 *   - int Java -> int32_t; char Java (unidade UTF-16) -> uint16_t;
 *     boolean -> bool (stdbool.h);
 *   - 'main' Java vira __j2c_main e um wrapper 'int main(void)' chama-o;
 *   - helpers de impressão só são emitidos quando usados;
 *   - reais usam __j2c_print_real, preservando 2.0/NaN/Infinity do Java.
 *
 * O modo limpo (codegen_emit_clean) usa a MESMA IR, mas prefere C idiomático:
 *   - int nativo, unsigned short para char e includes só quando necessários;
 *   - float/double impressos com printf("%g") (sem helper de real);
 *   - temporários de uso único inlinados, inclusive retornos de chamada
 *     consumidos imediatamente, e locais declarados na primeira atribuição;
 *   - 'main' direto quando não é chamado por outra função;
 *   - rótulos/gotos permanecem, pois o fluxo de controle vem da TAC.
 *
 * Em ambos: nomes da IR já são únicos/não reservados (ver ir_gen.c) e os
 * protótipos precedem as definições.
 */

/* Expressão simples que produz um temporário. Usada apenas no modo limpo
 * para substituir `tN = expr; ... use tN` pelo próprio `expr`. */
typedef struct {
    IrOpcode   op;              /* IR_ASSIGN, IR_UNARY, IR_BINOP ou IR_CAST */
    IrOperand  arg1;
    IrOperand  arg2;
    IrUnOp     unop;
    IrBinOp    binop;
    IrType     cast_type;
} TempExpr;

/* Chamada guardada para inlining seguro do resultado (modo limpo). */
typedef struct {
    IrOperand  func;
    IrOperand *args;
    size_t     arg_count;
} CallExpr;

typedef struct {
    int       defined;
    int       def_index;
    int       ref_count;
    int       ref_index;
    int       call_result;      /* temporário produzido por IR_CALL */
    int       call_inline;      /* chamada pode ir direto para o único uso */
    int       simple;           /* definição dada por uma expressão simples */
    int       safe;             /* pode ser inlining no modo limpo */
    TempExpr  expr;
    CallExpr  call;
} TempInfo;

/* Local que pode ser declarado já inicializado na primeira atribuição. */
typedef struct {
    int         has_decl_init;
    int         decl_index;
    IrType      type;
    const char *name;
} LocalInfo;

typedef struct {
    FILE            *out;
    const IrProgram *prog;
    int              need_real;  /* existe print de float/double */
    int              need_char;  /* existe print de char */
    int              need_math;  /* NAN/INFINITY ou helper de real */
    int              need_bool;  /* algum valor bool aparece no programa */
    int              failed;
    int              clean;      /* modo de saída legível */
    int              direct_main;     /* main Java vira o main C direto */
    int              in_direct_main;  /* emitindo o corpo desse main */
    IrOperand       *args;       /* operandos de IR_PARAM pendentes */
    size_t           arg_count;
    size_t           arg_cap;
    TempInfo        *temps;      /* informações por função (modo limpo) */
    int              temp_count;
    LocalInfo       *locals;     /* informações dos locais (modo limpo) */
    int              local_count;
    unsigned char   *expanding;  /* evita recursão ao expandir temporários */
    IrInstr        **instrs;     /* instruções indexadas da função atual */
    int              instr_count;
} CgCtx;

/* ===================== Utilitários ===================== */

static void cg_error(CgCtx *ctx, const char *fmt, ...) {
    va_list ap;
    fputs("ERRO: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    ctx->failed = 1;
}

static const char *cg_type(CgCtx *ctx, IrType t) {
    if (ctx->clean) {
        switch (t) {
        case TYPE_INT:     return "int";
        case TYPE_CHAR:    return "unsigned short"; /* unidade UTF-16 Java */
        case TYPE_FLOAT:   return "float";
        case TYPE_DOUBLE:  return "double";
        case TYPE_BOOLEAN: return "bool";
        case TYPE_VOID:    return "void";
        case TYPE_STRING:  return "const char *";
        default:           return "int";
        }
    }
    switch (t) {
    case TYPE_INT:     return "int32_t";
    case TYPE_FLOAT:   return "float";
    case TYPE_DOUBLE:  return "double";
    case TYPE_BOOLEAN: return "bool";
    case TYPE_CHAR:    return "uint16_t";
    case TYPE_VOID:    return "void";
    case TYPE_STRING:  return "const char *";
    default:           return "int32_t";
    }
}

/* Nomes reservados pelo C gerado (wrapper e helpers). O ir_gen.c já evita
 * que nomes do programa comecem com __j2c_; aqui apenas traduzimos 'main' e
 * '__init_globals'. No modo limpo, 'main' é o próprio main C quando não é
 * chamado por outro método. */
static const char *cg_func_name(CgCtx *ctx, const char *name) {
    if (!name) return "<anon>";
    if (strcmp(name, "main") == 0)
        return (ctx->clean && ctx->direct_main) ? "main" : "__j2c_main";
    if (strcmp(name, "__init_globals") == 0) return "__j2c_init_globals";
    return name;
}

static int cg_is_reserved_output_name(const char *name) {
    static const char *names[] = {
        "__j2c_main", "__j2c_init_globals", "__j2c_print_real",
        "__j2c_print_char", NULL
    };
    if (!name) return 0;
    for (int i = 0; names[i]; i++)
        if (strcmp(name, names[i]) == 0) return 1;
    return 0;
}

static const char *cg_binop(IrBinOp op);
static void cg_render_expr(CgCtx *ctx, const TempExpr *expr);
static void cg_render_call(CgCtx *ctx, const CallExpr *call);
static void cg_render_rhs(CgCtx *ctx, const IrInstr *ins);
static int cg_var_is_used(const IrFunc *f, const char *name);
static int cg_temp_is_used(const IrFunc *f, int id);

static int cg_temp_inlined(const CgCtx *ctx, int id) {
    return ctx->clean && ctx->temps && id >= 0 &&
           id < ctx->temp_count &&
           (ctx->temps[id].safe || ctx->temps[id].call_inline);
}

/* Chamada cujo resultado ninguém lê: pode virar statement, sem destino. */
static int cg_temp_dead_call(const CgCtx *ctx, int id) {
    return ctx->clean && ctx->temps && id >= 0 &&
           id < ctx->temp_count && ctx->temps[id].call_result &&
           ctx->temps[id].ref_count == 0;
}

/* Copia um operando que vai ficar guardado em TempExpr. */
static IrOperand cg_copy_operand(const IrOperand *op) {
    IrOperand copy = *op;
    if (op->kind == IR_OP_VAR || op->kind == IR_OP_LABEL ||
        op->kind == IR_OP_FUNC) {
        copy.name = op->name ? strdup(op->name) : NULL;
    }
    return copy;
}

static void cg_release_operand(IrOperand *op) {
    if (op->kind == IR_OP_VAR || op->kind == IR_OP_LABEL ||
        op->kind == IR_OP_FUNC) {
        free(op->name);
        op->name = NULL;
    }
}

static void cg_release_expr(TempExpr *expr) {
    cg_release_operand(&expr->arg1);
    cg_release_operand(&expr->arg2);
}

static int cg_is_simple_def(IrOpcode op) {
    return op == IR_ASSIGN || op == IR_UNARY || op == IR_BINOP ||
           op == IR_CAST;
}

/* Operandos que são fontes (não destinos) em uma instrução. */
static void cg_collect_ref(CgCtx *ctx, const IrOperand *op, int index) {
    if (op->kind != IR_OP_TEMP || op->id < 0 || op->id >= ctx->temp_count)
        return;
    TempInfo *info = &ctx->temps[op->id];
    info->ref_count++;
    info->ref_index = index;
}

/* O inlining só é feito quando o único uso é uma instrução que não muda a
 * ordem de avaliação (não é parâmetro de chamada nem a própria chamada). */
static int cg_safe_use_op(IrOpcode op) {
    return op == IR_ASSIGN || op == IR_UNARY || op == IR_BINOP ||
           op == IR_CAST || op == IR_RETURN || op == IR_IF_GOTO ||
           op == IR_IF_FALSE || op == IR_PRINT;
}

static int cg_append_arg(CgCtx *ctx, IrOperand **arr, size_t *count,
                         size_t *cap, IrOperand op) {
    if (*count == *cap) {
        size_t next = *cap ? *cap * 2 : 8;
        IrOperand *tmp = realloc(*arr, next * sizeof *tmp);
        if (!tmp) {
            cg_error(ctx, "sem memoria ao guardar argumentos de chamada");
            return 0;
        }
        *arr = tmp;
        *cap = next;
    }
    (*arr)[(*count)++] = op;
    return 1;
}

static void cg_release_args(IrOperand *arr, size_t count) {
    for (size_t i = 0; i < count; i++) cg_release_operand(&arr[i]);
    free(arr);
}

static int cg_is_control(IrOpcode op) {
    return op == IR_LABEL || op == IR_GOTO ||
           op == IR_IF_GOTO || op == IR_IF_FALSE;
}

static int cg_arg_uses_var(const IrInstr *ins, const char *name) {
    const IrOperand *ops[] = { &ins->arg1, &ins->arg2 };
    for (int i = 0; i < 2; i++)
        if (ops[i]->kind == IR_OP_VAR && ops[i]->name &&
            strcmp(ops[i]->name, name) == 0)
            return 1;
    return 0;
}

static int cg_can_init_at(const IrInstr *ins) {
    return ins->op == IR_ASSIGN || ins->op == IR_UNARY ||
           ins->op == IR_BINOP || ins->op == IR_CAST || ins->op == IR_CALL;
}

static void cg_build_clean_infos(CgCtx *ctx, const IrFunc *f) {
    int n = f->temp_count;

    ctx->instr_count = 0;
    for (IrInstr *ins = f->first; ins; ins = ins->next) ctx->instr_count++;
    ctx->instrs = ctx->instr_count
                      ? calloc((size_t)ctx->instr_count, sizeof *ctx->instrs)
                      : NULL;
    if (ctx->instr_count && !ctx->instrs) {
        cg_error(ctx, "sem memoria ao preparar o modo limpo");
        return;
    }

    if (n > 0) {
        ctx->temp_count = n;
        ctx->temps = calloc((size_t)n, sizeof *ctx->temps);
        ctx->expanding = calloc((size_t)n, sizeof *ctx->expanding);
        if (!ctx->temps || !ctx->expanding) {
            cg_error(ctx, "sem memoria ao preparar o modo limpo");
            return;
        }
    }

    IrOperand *pending = NULL;
    size_t pending_count = 0, pending_cap = 0;

    int i = 0;
    for (IrInstr *ins = f->first; ins; ins = ins->next, i++) {
        ctx->instrs[i] = ins;

        if (ins->op == IR_PARAM) {
            cg_append_arg(ctx, &pending, &pending_count, &pending_cap,
                          cg_copy_operand(&ins->arg1));
        } else if (ins->op == IR_CALL) {
            if (ins->res.kind == IR_OP_TEMP && ins->res.id >= 0 &&
                ins->res.id < n) {
                TempInfo *info = &ctx->temps[ins->res.id];
                info->call.func = cg_copy_operand(&ins->arg1);
                info->call.args = pending;
                info->call.arg_count = pending_count;
                pending = NULL;
                pending_count = pending_cap = 0;
            } else {
                cg_release_args(pending, pending_count);
                pending = NULL;
                pending_count = pending_cap = 0;
            }
        }

        if (ins->res.kind == IR_OP_TEMP && ins->res.id >= 0 &&
            ins->res.id < n) {
            TempInfo *info = &ctx->temps[ins->res.id];
            info->defined = 1;
            info->def_index = i;
            info->simple = cg_is_simple_def(ins->op);
            info->call_result = (ins->op == IR_CALL);
            info->expr.op = ins->op;
            info->expr.arg1 = cg_copy_operand(&ins->arg1);
            info->expr.arg2 = cg_copy_operand(&ins->arg2);
            info->expr.unop = ins->unop;
            info->expr.binop = ins->binop;
            info->expr.cast_type = ins->cast_type;
        }
        cg_collect_ref(ctx, &ins->arg1, i);
        cg_collect_ref(ctx, &ins->arg2, i);
    }
    cg_release_args(pending, pending_count);

    for (int id = 0; id < n; id++) {
        TempInfo *info = &ctx->temps[id];
        if (!info->defined || info->ref_count != 1) continue;

        if (info->call_result) {
            /* A chamada só vira expressão quando seu resultado é consumido
             * logo na instrução seguinte; com outra chamada pelo caminho a
             * ordem de avaliação poderia mudar. */
            if (info->ref_index == info->def_index + 1 &&
                cg_safe_use_op(ctx->instrs[info->ref_index]->op))
                info->call_inline = 1;
            continue;
        }
        if (!info->simple) continue;

        if (info->ref_index <= info->def_index ||
            !cg_safe_use_op(ctx->instrs[info->ref_index]->op))
            continue;

        info->safe = 1;
        for (int j = info->def_index + 1; j < info->ref_index; j++) {
            const IrInstr *mid = ctx->instrs[j];
            if (!(mid->res.kind == IR_OP_TEMP && cg_is_simple_def(mid->op))) {
                info->safe = 0;
                break;
            }
        }
    }

    /* Locais: declarar junto da primeira atribuição é seguro se não houver
     * uso, desvio ou rótulo antes dela e se a variável for lida depois. */
    ctx->local_count = 0;
    for (IrLocal *l = f->locals; l; l = l->next) ctx->local_count++;
    if (ctx->local_count > 0) {
        ctx->locals = calloc((size_t)ctx->local_count, sizeof *ctx->locals);
        if (!ctx->locals) {
            cg_error(ctx, "sem memoria ao preparar declarations do modo limpo");
            return;
        }
        int li = 0;
        for (IrLocal *l = f->locals; l; l = l->next, li++) {
            LocalInfo *local = &ctx->locals[li];
            local->name = l->name;
            local->type = l->type;
            if (!cg_var_is_used(f, l->name)) continue;

            int blocked = 0;
            int first_def = -1;
            for (int j = 0; j < ctx->instr_count; j++) {
                const IrInstr *ins = ctx->instrs[j];
                if (cg_is_control(ins->op) || cg_arg_uses_var(ins, l->name)) {
                    blocked = 1;
                    break;
                }
                if (first_def < 0 && ins->res.kind == IR_OP_VAR &&
                    ins->res.name && strcmp(ins->res.name, l->name) == 0 &&
                    cg_can_init_at(ins)) {
                    first_def = j;
                    break;
                }
            }
            if (!blocked && first_def >= 0) {
                local->has_decl_init = 1;
                local->decl_index = first_def;
            }
        }
    }
}

static void cg_free_clean_infos(CgCtx *ctx) {
    if (ctx->temps) {
        for (int i = 0; i < ctx->temp_count; i++) {
            TempInfo *info = &ctx->temps[i];
            cg_release_expr(&info->expr);
            cg_release_operand(&info->call.func);
            cg_release_args(info->call.args, info->call.arg_count);
        }
    }
    free(ctx->temps);
    free(ctx->expanding);
    free(ctx->instrs);
    free(ctx->locals);
    ctx->temps = NULL;
    ctx->expanding = NULL;
    ctx->instrs = NULL;
    ctx->locals = NULL;
    ctx->temp_count = 0;
    ctx->local_count = 0;
    ctx->instr_count = 0;
}

static int cg_var_is_used(const IrFunc *f, const char *name) {
    if (!name) return 0;
    for (const IrInstr *ins = f->first; ins; ins = ins->next) {
        const IrOperand *ops[] = { &ins->arg1, &ins->arg2 };
        for (int i = 0; i < 2; i++) {
            if (ops[i]->kind == IR_OP_VAR && ops[i]->name &&
                strcmp(ops[i]->name, name) == 0)
                return 1;
        }
    }
    return 0;
}

static int cg_temp_is_used(const IrFunc *f, int id) {
    for (const IrInstr *ins = f->first; ins; ins = ins->next) {
        const IrOperand *ops[] = { &ins->arg1, &ins->arg2 };
        for (int i = 0; i < 2; i++)
            if (ops[i]->kind == IR_OP_TEMP && ops[i]->id == id) return 1;
    }
    return 0;
}

static void cg_real_literal(CgCtx *ctx, double v, IrType type) {
    char buf[64];

    if (isnan(v)) {
        fputs("NAN", ctx->out);
        return;
    }
    if (isinf(v)) {
        fputs(v > 0 ? "INFINITY" : "(-INFINITY)", ctx->out);
        return;
    }

    int is_float = (type == TYPE_FLOAT);
    int max_prec = is_float ? 9 : 17;
    for (int prec = 1; prec <= max_prec; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, v);
        double back = strtod(buf, NULL);
        if (is_float ? ((float)back == (float)v) : (back == v)) break;
    }
    if (!strpbrk(buf, ".eEn")) strcat(buf, ".0");
    fputs(buf, ctx->out);
    if (is_float) fputc('f', ctx->out);
}

static void cg_operand(CgCtx *ctx, const IrOperand *op) {
    if (!op) {
        fputs("0", ctx->out);
        return;
    }
    switch (op->kind) {
    case IR_OP_EMPTY:
        fputs("0", ctx->out);
        break;
    case IR_OP_TEMP:
        if (ctx->clean && ctx->temps && op->id >= 0 &&
            op->id < ctx->temp_count && !ctx->expanding[op->id]) {
            TempInfo *info = &ctx->temps[op->id];
            if (info->call_inline) {
                ctx->expanding[op->id] = 1;
                cg_render_call(ctx, &info->call);
                ctx->expanding[op->id] = 0;
            } else if (info->safe) {
                ctx->expanding[op->id] = 1;
                cg_render_expr(ctx, &info->expr);
                ctx->expanding[op->id] = 0;
            } else {
                fprintf(ctx->out, "t%d", op->id);
            }
        } else {
            fprintf(ctx->out, "t%d", op->id);
        }
        break;
    case IR_OP_VAR:
    case IR_OP_LABEL:
        fputs(op->name ? op->name : "?", ctx->out);
        break;
    case IR_OP_FUNC:
        fputs(cg_func_name(ctx, op->name), ctx->out);
        break;
    case IR_OP_CONST:
        if (op->type == TYPE_FLOAT || op->type == TYPE_DOUBLE)
            cg_real_literal(ctx, op->value.d, op->type);
        else if (op->type == TYPE_BOOLEAN)
            fputs(op->value.i ? "1" : "0", ctx->out);
        else
            fprintf(ctx->out, "%ld", op->value.i);
        break;
    }
}

/* Destino de uma instrução nunca é expandido: apenas fontes podem virar
 * expressões inline no modo limpo. */
static void cg_dest(CgCtx *ctx, const IrOperand *op) {
    if (!op) {
        fputs("0", ctx->out);
    } else if (op->kind == IR_OP_TEMP) {
        fprintf(ctx->out, "t%d", op->id);
    } else if (op->kind == IR_OP_EMPTY) {
        fputs("0", ctx->out);
    } else {
        cg_operand(ctx, op);
    }
}

static void cg_render_expr(CgCtx *ctx, const TempExpr *expr) {
    switch (expr->op) {
    case IR_ASSIGN:
        cg_operand(ctx, &expr->arg1);
        break;
    case IR_UNARY:
        fputc('(', ctx->out);
        fputs(expr->unop == OP_NOT ? "!" : "-", ctx->out);
        cg_operand(ctx, &expr->arg1);
        fputc(')', ctx->out);
        break;
    case IR_BINOP:
        fputc('(', ctx->out);
        cg_operand(ctx, &expr->arg1);
        fprintf(ctx->out, " %s ", cg_binop(expr->binop));
        cg_operand(ctx, &expr->arg2);
        fputc(')', ctx->out);
        break;
    case IR_CAST:
        fputc('(', ctx->out);
        fprintf(ctx->out, "(%s)", cg_type(ctx, expr->cast_type));
        cg_operand(ctx, &expr->arg1);
        fputc(')', ctx->out);
        break;
    default:
        fputs("0", ctx->out);
        break;
    }
}

static void cg_render_call(CgCtx *ctx, const CallExpr *call) {
    cg_operand(ctx, &call->func);
    fputc('(', ctx->out);
    for (size_t i = 0; i < call->arg_count; i++) {
        if (i) fputs(", ", ctx->out);
        cg_operand(ctx, &call->args[i]);
    }
    fputc(')', ctx->out);
}

/* RHS de uma instrução que inicializa um local na própria declaração. */
static void cg_render_rhs(CgCtx *ctx, const IrInstr *ins) {
    if (ins->op == IR_CALL) {
        CallExpr call;
        call.func = ins->arg1;
        call.args = ctx->args;
        call.arg_count = ctx->arg_count;
        cg_render_call(ctx, &call);
        return;
    }
    TempExpr expr;
    memset(&expr, 0, sizeof expr);
    expr.op = ins->op;
    expr.arg1 = ins->arg1;
    expr.arg2 = ins->arg2;
    expr.unop = ins->unop;
    expr.binop = ins->binop;
    expr.cast_type = ins->cast_type;
    cg_render_expr(ctx, &expr);
}

static void cg_default_value(CgCtx *ctx, IrType type) {
    if (type == TYPE_BOOLEAN) {
        fputs("0", ctx->out);
    } else if (type == TYPE_FLOAT) {
        fputs("0.0f", ctx->out);
    } else if (type == TYPE_DOUBLE) {
        fputs("0.0", ctx->out);
    } else {
        fputs("0", ctx->out);
    }
}

/* ===================== Validação / varredura ===================== */

static void cg_note_type(CgCtx *ctx, IrType type) {
    if (type == TYPE_BOOLEAN) ctx->need_bool = 1;
}

static void cg_scan_operand(CgCtx *ctx, const IrOperand *op) {
    if (!op) return;
    cg_note_type(ctx, op->type);
    if (op->kind != IR_OP_CONST) return;
    if (op->type != TYPE_FLOAT && op->type != TYPE_DOUBLE) return;
    if (isnan(op->value.d) || isinf(op->value.d)) ctx->need_math = 1;
}

static int cg_scan(CgCtx *ctx) {
    for (IrGlobal *g = ctx->prog->globals; g; g = g->next) {
        if (strcmp(g->name, "main") == 0 || cg_is_reserved_output_name(g->name))
            cg_error(ctx, "nome de global reservado pelo C gerado: %s", g->name);
        cg_note_type(ctx, g->type);
        cg_scan_operand(ctx, &g->init);
    }

    for (IrFunc *f = ctx->prog->funcs; f; f = f->next) {
        if (cg_is_reserved_output_name(f->name))
            cg_error(ctx, "nome de funcao reservado pelo C gerado: %s", f->name);
        cg_note_type(ctx, f->ret_type);
        for (IrParam *p = f->params; p; p = p->next) cg_note_type(ctx, p->type);
        for (IrLocal *l = f->locals; l; l = l->next) cg_note_type(ctx, l->type);

        size_t pending = 0;
        for (IrInstr *ins = f->first; ins; ins = ins->next) {
            cg_note_type(ctx, ins->type);
            cg_note_type(ctx, ins->cast_type);
            cg_scan_operand(ctx, &ins->res);
            cg_scan_operand(ctx, &ins->arg1);
            cg_scan_operand(ctx, &ins->arg2);

            if (ins->op == IR_PARAM) {
                pending++;
            } else if (ins->op == IR_CALL) {
                int nargs = -1;
                if (ins->arg2.kind == IR_OP_CONST)
                    nargs = (int)ins->arg2.value.i;
                if (nargs < 0 || (size_t)nargs != pending)
                    cg_error(ctx, "IR_CALL com %d argumentos, mas %zu IR_PARAM",
                             nargs, pending);
                pending = 0;
            }

            if (ins->op == IR_PRINT && !ins->print_string) {
                if (ins->arg1.type == TYPE_FLOAT || ins->arg1.type == TYPE_DOUBLE)
                    ctx->need_real = 1;
                else if (ins->arg1.type == TYPE_CHAR)
                    ctx->need_char = 1;
                cg_scan_operand(ctx, &ins->arg1);
            }
        }
        if (pending != 0)
            cg_error(ctx, "IR_PARAM sem IR_CALL na funcao %s", f->name);
    }

    /* No modo bruto o helper de reais usa isinf/isnan; no limpo, `%g`
     * não precisa de math.h (a menos que exista literal NaN/Infinity). */
    if (!ctx->clean && ctx->need_real) ctx->need_math = 1;
    return !ctx->failed;
}

/* ===================== Cabeçalho e declarações ===================== */

static void cg_emit_includes(CgCtx *ctx) {
    if (ctx->clean) {
        fputs("/* Codigo C gerado por java2c (modo limpo). */\n", ctx->out);
        fputs("#include <stdio.h>\n", ctx->out);
        if (ctx->need_bool) fputs("#include <stdbool.h>\n", ctx->out);
        if (ctx->need_math) fputs("#include <math.h>\n", ctx->out);
        fputc('\n', ctx->out);
        return;
    }
    fputs("/* Codigo C gerado por java2c a partir da IR. Nao editar. */\n", ctx->out);
    fputs("#include <stdbool.h>\n", ctx->out);
    fputs("#include <stdint.h>\n", ctx->out);
    fputs("#include <stdio.h>\n", ctx->out);
    if (ctx->need_real) {
        fputs("#include <stdlib.h>\n", ctx->out);
        fputs("#include <string.h>\n", ctx->out);
    }
    if (ctx->need_math) fputs("#include <math.h>\n", ctx->out);
    fputc('\n', ctx->out);
}

static void cg_emit_globals(CgCtx *ctx) {
    for (IrGlobal *g = ctx->prog->globals; g; g = g->next) {
        fprintf(ctx->out, "%s %s = ", cg_type(ctx, g->type), g->name);
        if (g->has_init && g->init.kind == IR_OP_CONST)
            cg_operand(ctx, &g->init);
        else
            cg_default_value(ctx, g->type);
        fputs(";\n", ctx->out);
    }
}

static void cg_emit_prototype(CgCtx *ctx, const char *ret, const char *name,
                              const IrParam *params) {
    fprintf(ctx->out, "%s %s(", ret, name);
    if (!params) {
        fputs("void", ctx->out);
    } else {
        int first = 1;
        for (; params; params = params->next) {
            if (!first) fputs(", ", ctx->out);
            fprintf(ctx->out, "%s %s", cg_type(ctx, params->type), params->name);
            first = 0;
        }
    }
    fputs(");\n", ctx->out);
}

/* Devolve 1 quando algum protótipo foi escrito (para controlar linhas em
 * branco). No modo bruto sempre há pelo menos o protótipo de main. */
static int cg_emit_prototypes(CgCtx *ctx) {
    if (ctx->clean) {
        int any = 0;
        if (ctx->need_char) {
            fputs("static void __j2c_print_char(unsigned short c);\n", ctx->out);
            any = 1;
        }
        for (IrFunc *f = ctx->prog->funcs; f; f = f->next) {
            if (ctx->direct_main && strcmp(f->name, "main") == 0) continue;
            cg_emit_prototype(ctx, cg_type(ctx, f->ret_type),
                              cg_func_name(ctx, f->name), f->params);
            any = 1;
        }
        return any;
    }

    if (ctx->need_real)
        fputs("static void __j2c_print_real(double v, int is_float);\n", ctx->out);
    if (ctx->need_char)
        fputs("static void __j2c_print_char(uint16_t c);\n", ctx->out);

    for (IrFunc *f = ctx->prog->funcs; f; f = f->next)
        cg_emit_prototype(ctx, cg_type(ctx, f->ret_type),
                          cg_func_name(ctx, f->name), f->params);
    fputs("int main(void);\n", ctx->out);
    return 1;
}

/* ===================== Helpers de impressão ===================== */

static void cg_emit_helpers(CgCtx *ctx) {
    if (ctx->clean) {
        /* Reais usam printf("%g") direto: sem math.h, snprintf ou helper. */
        if (ctx->need_char) {
            fputs("static void __j2c_print_char(unsigned short c)\n", ctx->out);
            fputs("{\n", ctx->out);
            fputs("    if (c < 0x80) {\n", ctx->out);
            fputs("        putchar((int)c);\n", ctx->out);
            fputs("    } else if (c < 0x800) {\n", ctx->out);
            fputs("        putchar((int)(0xC0 | (c >> 6)));\n", ctx->out);
            fputs("        putchar((int)(0x80 | (c & 0x3F)));\n", ctx->out);
            fputs("    } else {\n", ctx->out);
            fputs("        putchar((int)(0xE0 | (c >> 12)));\n", ctx->out);
            fputs("        putchar((int)(0x80 | ((c >> 6) & 0x3F)));\n", ctx->out);
            fputs("        putchar((int)(0x80 | (c & 0x3F)));\n", ctx->out);
            fputs("    }\n", ctx->out);
            fputs("    putchar('\\n');\n", ctx->out);
            fputs("}\n\n", ctx->out);
        }
        return;
    }

    if (ctx->need_real) {
        fputs("static void __j2c_print_real(double v, int is_float)\n", ctx->out);
        fputs("{\n", ctx->out);
        fputs("    char buf[64];\n", ctx->out);
        fputs("    int max_prec;\n", ctx->out);
        fputs("    int prec;\n", ctx->out);
        fputs("\n", ctx->out);
        fputs("    if (isnan(v)) { puts(\"NaN\"); return; }\n", ctx->out);
        fputs("    if (isinf(v)) { puts(v > 0 ? \"Infinity\" : \"-Infinity\"); return; }\n", ctx->out);
        fputs("    if (is_float) v = (float)v;\n", ctx->out);
        fputs("    max_prec = is_float ? 9 : 17;\n", ctx->out);
        fputs("    for (prec = 1; prec <= max_prec; prec++) {\n", ctx->out);
        fputs("        snprintf(buf, sizeof buf, \"%.*g\", prec, v);\n", ctx->out);
        fputs("        double back = strtod(buf, NULL);\n", ctx->out);
        fputs("        if (is_float ? ((float)back == (float)v) : (back == v)) break;\n", ctx->out);
        fputs("    }\n", ctx->out);
        fputs("    if (!strpbrk(buf, \".eEn\")) strcat(buf, \".0\");\n", ctx->out);
        fputs("    printf(\"%s\\n\", buf);\n", ctx->out);
        fputs("}\n\n", ctx->out);
    }

    if (ctx->need_char) {
        fputs("static void __j2c_print_char(uint16_t c)\n", ctx->out);
        fputs("{\n", ctx->out);
        fputs("    if (c < 0x80) {\n", ctx->out);
        fputs("        putchar((int)c);\n", ctx->out);
        fputs("    } else if (c < 0x800) {\n", ctx->out);
        fputs("        putchar((int)(0xC0 | (c >> 6)));\n", ctx->out);
        fputs("        putchar((int)(0x80 | (c & 0x3F)));\n", ctx->out);
        fputs("    } else {\n", ctx->out);
        fputs("        putchar((int)(0xE0 | (c >> 12)));\n", ctx->out);
        fputs("        putchar((int)(0x80 | ((c >> 6) & 0x3F)));\n", ctx->out);
        fputs("        putchar((int)(0x80 | (c & 0x3F)));\n", ctx->out);
        fputs("    }\n", ctx->out);
        fputs("    putchar('\\n');\n", ctx->out);
        fputs("}\n\n", ctx->out);
    }
}

/* ===================== Instruções ===================== */

static void cg_push_arg(CgCtx *ctx, IrOperand op) {
    if (ctx->arg_count == ctx->arg_cap) {
        size_t cap = ctx->arg_cap ? ctx->arg_cap * 2 : 8;
        IrOperand *tmp = realloc(ctx->args, cap * sizeof *tmp);
        if (!tmp) {
            cg_error(ctx, "sem memoria ao alocar argumentos de chamada");
            return;
        }
        ctx->args = tmp;
        ctx->arg_cap = cap;
    }
    ctx->args[ctx->arg_count++] = op;
}

static void cg_emit_call(CgCtx *ctx, const IrInstr *ins) {
    fputs("    ", ctx->out);
    int dead_result = ins->res.kind == IR_OP_TEMP && ins->res.id >= 0 &&
                      ins->res.id < ctx->temp_count &&
                      cg_temp_dead_call(ctx, ins->res.id);
    if (ins->res.kind != IR_OP_EMPTY && !dead_result) {
        cg_dest(ctx, &ins->res);
        fputs(" = ", ctx->out);
    }
    cg_operand(ctx, &ins->arg1);
    fputc('(', ctx->out);
    for (size_t i = 0; i < ctx->arg_count; i++) {
        if (i) fputs(", ", ctx->out);
        cg_operand(ctx, &ctx->args[i]);
    }
    fputs(");\n", ctx->out);
    ctx->arg_count = 0;
}

static const char *cg_binop(IrBinOp op) {
    switch (op) {
    case OP_ADD: return "+";
    case OP_SUB: return "-";
    case OP_MULT: return "*";
    case OP_DIV: return "/";
    case OP_MOD: return "%";
    case OP_LT: return "<";
    case OP_GT: return ">";
    case OP_LE: return "<=";
    case OP_GE: return ">=";
    case OP_EQ: return "==";
    case OP_NE: return "!=";
    case OP_AND: return "&&";
    case OP_OR: return "||";
    default: return "?";
    }
}

static void cg_emit_print(CgCtx *ctx, const IrInstr *ins) {
    if (ins->print_string) {
        /* '%s' evita que um '%' no literal Java seja lido como specifier. */
        fputs("    printf(\"%s\\n\", \"", ctx->out);
        fputs(ins->print_string, ctx->out);
        fputs("\");\n", ctx->out);
        return;
    }

    if (ctx->clean) {
        switch (ins->arg1.type) {
        case TYPE_BOOLEAN:
            fputs("    printf(\"%s\\n\", ", ctx->out);
            cg_operand(ctx, &ins->arg1);
            fputs(" ? \"true\" : \"false\");\n", ctx->out);
            break;
        case TYPE_CHAR:
            fputs("    __j2c_print_char(", ctx->out);
            cg_operand(ctx, &ins->arg1);
            fputs(");\n", ctx->out);
            break;
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            fputs("    printf(\"%g\\n\", ", ctx->out);
            cg_operand(ctx, &ins->arg1);
            fputs(");\n", ctx->out);
            break;
        default:
            fputs("    printf(\"%d\\n\", ", ctx->out);
            cg_operand(ctx, &ins->arg1);
            fputs(");\n", ctx->out);
            break;
        }
        return;
    }

    switch (ins->arg1.type) {
    case TYPE_BOOLEAN:
        fputs("    printf(\"%s\\n\", ", ctx->out);
        cg_operand(ctx, &ins->arg1);
        fputs(" ? \"true\" : \"false\");\n", ctx->out);
        break;
    case TYPE_CHAR:
        fputs("    __j2c_print_char(", ctx->out);
        cg_operand(ctx, &ins->arg1);
        fputs(");\n", ctx->out);
        break;
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        fputs("    __j2c_print_real(", ctx->out);
        cg_operand(ctx, &ins->arg1);
        fprintf(ctx->out, ", %d);\n", ins->arg1.type == TYPE_FLOAT);
        break;
    default:
        fputs("    printf(\"%d\\n\", ", ctx->out);
        cg_operand(ctx, &ins->arg1);
        fputs(");\n", ctx->out);
        break;
    }
}

static void cg_emit_instr(CgCtx *ctx, const IrInstr *ins, int index) {
    if (ctx->clean) {
        /* Declarar o local já inicializado na primeira atribuição. */
        for (int li = 0; li < ctx->local_count; li++) {
            LocalInfo *local = &ctx->locals[li];
            if (!local->has_decl_init || local->decl_index != index) continue;
            fputs("    ", ctx->out);
            fprintf(ctx->out, "%s %s = ", cg_type(ctx, local->type), local->name);
            cg_render_rhs(ctx, ins);
            fputs(";\n", ctx->out);
            if (ins->op == IR_CALL) ctx->arg_count = 0;
            return;
        }

        /* A definição de um temporário inlinado some: o valor aparece
         * diretamente no único ponto onde é usado. */
        if (cg_is_simple_def(ins->op) && ins->res.kind == IR_OP_TEMP &&
            cg_temp_inlined(ctx, ins->res.id))
            return;
        if (ins->op == IR_CALL && ins->res.kind == IR_OP_TEMP &&
            ins->res.id >= 0 && ins->res.id < ctx->temp_count &&
            ctx->temps[ins->res.id].call_inline) {
            ctx->arg_count = 0; /* a chamada foi guardada em TempInfo.call */
            return;
        }
    }

    switch (ins->op) {
    case IR_NOP:
        break;
    case IR_ASSIGN:
        fputs("    ", ctx->out);
        cg_dest(ctx, &ins->res);
        fputs(" = ", ctx->out);
        cg_operand(ctx, &ins->arg1);
        fputs(";\n", ctx->out);
        break;
    case IR_UNARY:
        fputs("    ", ctx->out);
        cg_dest(ctx, &ins->res);
        fprintf(ctx->out, " = %s", ins->unop == OP_NOT ? "!" : "-");
        cg_operand(ctx, &ins->arg1);
        fputs(";\n", ctx->out);
        break;
    case IR_BINOP:
        fputs("    ", ctx->out);
        cg_dest(ctx, &ins->res);
        fprintf(ctx->out, " = ");
        cg_operand(ctx, &ins->arg1);
        fprintf(ctx->out, " %s ", cg_binop(ins->binop));
        cg_operand(ctx, &ins->arg2);
        fputs(";\n", ctx->out);
        break;
    case IR_CAST:
        fputs("    ", ctx->out);
        cg_dest(ctx, &ins->res);
        fprintf(ctx->out, " = (%s)", cg_type(ctx, ins->cast_type));
        cg_operand(ctx, &ins->arg1);
        fputs(";\n", ctx->out);
        break;
    case IR_LABEL:
        cg_operand(ctx, &ins->res);
        fputs(": ;\n", ctx->out);
        break;
    case IR_GOTO:
        fputs("    goto ", ctx->out);
        cg_operand(ctx, &ins->res);
        fputs(";\n", ctx->out);
        break;
    case IR_IF_GOTO:
        fputs("    if (", ctx->out);
        cg_operand(ctx, &ins->arg1);
        fputs(") goto ", ctx->out);
        cg_operand(ctx, &ins->res);
        fputs(";\n", ctx->out);
        break;
    case IR_IF_FALSE:
        fputs("    if (!", ctx->out);
        cg_operand(ctx, &ins->arg1);
        fputs(") goto ", ctx->out);
        cg_operand(ctx, &ins->res);
        fputs(";\n", ctx->out);
        break;
    case IR_PARAM:
        cg_push_arg(ctx, ins->arg1);
        break;
    case IR_CALL:
        cg_emit_call(ctx, ins);
        break;
    case IR_RETURN:
        fputs("    return", ctx->out);
        if (ins->arg1.kind != IR_OP_EMPTY) {
            fputc(' ', ctx->out);
            cg_operand(ctx, &ins->arg1);
        } else if (ctx->clean && ctx->in_direct_main) {
            fputs(" 0", ctx->out); /* 'return;' do main Java vira 'return 0;' */
        }
        fputs(";\n", ctx->out);
        break;
    case IR_PRINT:
        cg_emit_print(ctx, ins);
        break;
    }
}

/* ===================== Funções ===================== */

static IrType *cg_temp_types(const IrFunc *f) {
    if (f->temp_count <= 0) return NULL;
    IrType *types = calloc((size_t)f->temp_count, sizeof *types);
    if (!types) return NULL;
    for (const IrInstr *ins = f->first; ins; ins = ins->next) {
        if (ins->res.kind == IR_OP_TEMP && ins->res.id >= 0 &&
            ins->res.id < f->temp_count && types[ins->res.id] == TYPE_UNKNOWN)
            types[ins->res.id] = ins->res.type;
    }
    return types;
}

static int cg_local_initialized(CgCtx *ctx, const char *name) {
    if (!ctx->clean || !name) return 0;
    for (int i = 0; i < ctx->local_count; i++)
        if (ctx->locals[i].has_decl_init &&
            strcmp(ctx->locals[i].name, name) == 0)
            return 1;
    return 0;
}

static void cg_emit_unused_casts(CgCtx *ctx, const IrFunc *f) {
    int any = 0;
    for (IrParam *p = f->params; p; p = p->next) {
        if (ctx->clean && cg_var_is_used(f, p->name)) continue;
        if (any) fputc(' ', ctx->out);
        else fputs("    ", ctx->out);
        fprintf(ctx->out, "(void)%s;", p->name);
        any = 1;
    }
    for (IrLocal *l = f->locals; l; l = l->next) {
        if (ctx->clean &&
            (cg_var_is_used(f, l->name) || cg_local_initialized(ctx, l->name)))
            continue;
        if (any) fputc(' ', ctx->out);
        else fputs("    ", ctx->out);
        fprintf(ctx->out, "(void)%s;", l->name);
        any = 1;
    }
    for (int i = 0; i < f->temp_count; i++) {
        if (ctx->clean &&
            (cg_temp_inlined(ctx, i) || cg_temp_dead_call(ctx, i) ||
             cg_temp_is_used(f, i)))
            continue;
        if (any) fputc(' ', ctx->out);
        else fputs("    ", ctx->out);
        fprintf(ctx->out, "(void)t%d;", i);
        any = 1;
    }
    if (any) fputc('\n', ctx->out);
}

static void cg_emit_function(CgCtx *ctx, const IrFunc *f) {
    if (ctx->clean) cg_build_clean_infos(ctx, f);

    int direct = ctx->clean && ctx->direct_main &&
                 strcmp(f->name, "main") == 0;
    int was_in_direct_main = ctx->in_direct_main;
    ctx->in_direct_main = direct;

    /* 'main' direto sempre devolve int em C, mesmo quando o Java é void. */
    const char *ret = direct ? "int" : cg_type(ctx, f->ret_type);
    fprintf(ctx->out, "%s %s(", ret, cg_func_name(ctx, f->name));
    if (!f->params) {
        fputs("void", ctx->out);
    } else {
        int first = 1;
        for (IrParam *p = f->params; p; p = p->next) {
            if (!first) fputs(", ", ctx->out);
            fprintf(ctx->out, "%s %s", cg_type(ctx, p->type), p->name);
            first = 0;
        }
    }
    fputs(")\n{\n", ctx->out);

    int declarations = 0;
    for (IrLocal *l = f->locals; l; l = l->next) {
        if (ctx->clean && cg_local_initialized(ctx, l->name)) continue;
        fprintf(ctx->out, "    %s %s;\n", cg_type(ctx, l->type), l->name);
        declarations = 1;
    }

    IrType *types = cg_temp_types(f);
    if (!types && f->temp_count > 0)
        cg_error(ctx, "sem memoria ao declarar temporarios de %s", f->name);
    if (types) {
        for (int i = 0; i < f->temp_count; i++) {
            if (ctx->clean &&
                (cg_temp_inlined(ctx, i) || cg_temp_dead_call(ctx, i)))
                continue;
            fprintf(ctx->out, "    %s t%d;\n", cg_type(ctx, types[i]), i);
            declarations = 1;
        }
    }

    cg_emit_unused_casts(ctx, f);
    if (ctx->clean ? declarations : (f->locals || f->temp_count > 0))
        fputc('\n', ctx->out);

    int index = 0;
    for (IrInstr *ins = f->first; ins; ins = ins->next, index++)
        cg_emit_instr(ctx, ins, index);

    if (direct) fputs("    return 0;\n", ctx->out);

    fputs("}\n\n", ctx->out);
    free(types);
    ctx->in_direct_main = was_in_direct_main;
    if (ctx->clean) cg_free_clean_infos(ctx);
}

static int cg_main_is_called(const IrProgram *p) {
    for (const IrFunc *f = p->funcs; f; f = f->next) {
        for (const IrInstr *ins = f->first; ins; ins = ins->next) {
            if (ins->op == IR_CALL && ins->arg1.kind == IR_OP_FUNC &&
                ins->arg1.name && strcmp(ins->arg1.name, "main") == 0)
                return 1;
        }
    }
    return 0;
}

static void cg_emit_main_wrapper(CgCtx *ctx) {
    if (ctx->direct_main) return;

    const IrFunc *main_func = NULL;
    const IrFunc *init_func = NULL;
    for (const IrFunc *f = ctx->prog->funcs; f; f = f->next) {
        if (strcmp(f->name, "main") == 0) main_func = f;
        else if (strcmp(f->name, "__init_globals") == 0) init_func = f;
    }

    fputs("int main(void)\n{\n", ctx->out);
    if (init_func)
        fprintf(ctx->out, "    %s();\n", cg_func_name(ctx, init_func->name));
    if (main_func) {
        const char *name = cg_func_name(ctx, main_func->name);
        if (main_func->ret_type == TYPE_VOID)
            fprintf(ctx->out, "    %s();\n", name);
        else
            fprintf(ctx->out, "    (void)%s();\n", name);
    }
    fputs("    return 0;\n}\n", ctx->out);
}

/* ===================== API ===================== */

static int cg_emit(FILE *out, const IrProgram *program, int clean) {
    if (!out || !program) return -1;

    CgCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.out = out;
    ctx.prog = program;
    ctx.clean = clean;

    /* No modo limpo o main Java é o próprio main C, desde que nenhuma
     * outra função o chame (nesse caso ele continua sendo uma função
     * comum e o wrapper permanece). */
    if (clean) {
        int has_main = 0;
        for (const IrFunc *f = program->funcs; f; f = f->next)
            if (f->name && strcmp(f->name, "main") == 0) has_main = 1;
        ctx.direct_main = has_main && !cg_main_is_called(program);
    }

    if (!cg_scan(&ctx)) {
        free(ctx.args);
        return -1;
    }

    cg_emit_includes(&ctx);
    cg_emit_globals(&ctx);
    if (program->globals) fputc('\n', ctx.out);
    if (cg_emit_prototypes(&ctx)) fputc('\n', ctx.out);
    cg_emit_helpers(&ctx);

    for (IrFunc *f = program->funcs; f; f = f->next)
        cg_emit_function(&ctx, f);
    cg_emit_main_wrapper(&ctx);

    if (ferror(ctx.out)) ctx.failed = 1;
    cg_free_clean_infos(&ctx); /* não faz nada no modo normal */
    free(ctx.args);
    return ctx.failed ? -1 : 0;
}

int codegen_emit(FILE *out, const IrProgram *program) {
    return cg_emit(out, program, 0);
}

int codegen_emit_clean(FILE *out, const IrProgram *program) {
    return cg_emit(out, program, 1);
}
