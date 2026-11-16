#include "ir.h"

#include <stdlib.h>
#include <string.h>

/* ===================== Utilitários internos ===================== */

static char *ir_strdup(const char *s) {
    if (!s) return NULL;
    char *p = strdup(s);
    if (!p) {
        fprintf(stderr, "ERRO: sem memoria ao duplicar string na IR.\n");
        exit(1);
    }
    return p;
}

static void *ir_calloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (!p) {
        fprintf(stderr, "ERRO: sem memoria ao alocar estrutura da IR.\n");
        exit(1);
    }
    return p;
}

const char *ir_type_name(IrType t) {
    return type_name(t);
}

/* ===================== Programa / funções / globais ===================== */

IrProgram *ir_program_new(void) {
    IrProgram *p = ir_calloc(1, sizeof *p);
    p->globals = NULL;
    p->funcs = NULL;
    p->label_count = 0;
    p->temp_count = 0;
    return p;
}

static void ir_free_params(IrParam *pr) {
    while (pr) {
        IrParam *next = pr->next;
        free(pr->name);
        free(pr);
        pr = next;
    }
}

void ir_operand_release(IrOperand *op) {
    if (!op) return;
    if (op->kind == IR_OP_VAR || op->kind == IR_OP_LABEL || op->kind == IR_OP_FUNC) {
        free(op->name);
        op->name = NULL;
    }
}

static void ir_free_instr(IrInstr *ins) {
    ir_operand_release(&ins->res);
    ir_operand_release(&ins->arg1);
    ir_operand_release(&ins->arg2);
    free(ins->print_string);
    free(ins);
}

static void ir_free_func(IrFunc *f) {
    ir_free_params(f->params);
    IrInstr *ins = f->first;
    while (ins) {
        IrInstr *next = ins->next;
        ir_free_instr(ins);
        ins = next;
    }
    free(f->name);
    free(f);
}

void ir_program_free(IrProgram *p) {
    if (!p) return;
    IrGlobal *g = p->globals;
    while (g) {
        IrGlobal *next = g->next;
        free(g->name);
        ir_operand_release(&g->init);
        free(g);
        g = next;
    }
    IrFunc *f = p->funcs;
    while (f) {
        IrFunc *next = f->next;
        ir_free_func(f);
        f = next;
    }
    free(p);
}

IrFunc *ir_program_add_func(IrProgram *p, const char *name, IrType ret_type) {
    IrFunc *f = ir_calloc(1, sizeof *f);
    f->name = ir_strdup(name ? name : "<anon>");
    f->ret_type = ret_type;
    f->params = NULL;
    f->first = f->last = NULL;
    f->temp_count = 0;
    f->next = NULL;

    if (!p->funcs) {
        p->funcs = f;
    } else {
        IrFunc *it = p->funcs;
        while (it->next) it = it->next;
        it->next = f;
    }
    return f;
}

void ir_func_add_param(IrFunc *f, const char *name, IrType type) {
    IrParam *pr = ir_calloc(1, sizeof *pr);
    pr->name = ir_strdup(name);
    pr->type = type;
    pr->next = NULL;
    if (!f->params) {
        f->params = pr;
    } else {
        IrParam *it = f->params;
        while (it->next) it = it->next;
        it->next = pr;
    }
}

void ir_program_add_global(IrProgram *p, const char *name, IrType type,
                           int has_init, IrOperand init) {
    /* Evita duplicar o mesmo global (programas "soltos" podem expor o
     * mesmo nome em mais de um ponto conceitual; mantemos a 1a definição). */
    for (IrGlobal *it = p->globals; it; it = it->next) {
        if (strcmp(it->name, name) == 0) {
            /* Se a nova definição trouxer inicializador e a antiga não,
             * atualiza para preservar informação útil. */
            if (!it->has_init && has_init) {
                it->has_init = 1;
                it->init = init; /* assume posse de strings do operando */
            } else {
                /* não vamos usar 'init': libera nome duplicado se houver */
                IrOperand tmp = init;
                ir_operand_release(&tmp);
            }
            return;
        }
    }
    IrGlobal *g = ir_calloc(1, sizeof *g);
    g->name = ir_strdup(name);
    g->type = type;
    g->has_init = has_init;
    g->init = init;
    g->next = NULL;
    if (!p->globals) {
        p->globals = g;
    } else {
        IrGlobal *it = p->globals;
        while (it->next) it = it->next;
        it->next = g;
    }
}

/* ===================== Criação de operandos ===================== */

IrOperand ir_empty(void) {
    IrOperand op;
    memset(&op, 0, sizeof op);
    op.kind = IR_OP_EMPTY;
    op.type = TYPE_UNKNOWN;
    op.id = -1;
    op.name = NULL;
    return op;
}

IrOperand ir_temp(int id, IrType type) {
    IrOperand op = ir_empty();
    op.kind = IR_OP_TEMP;
    op.type = type;
    op.id = id;
    return op;
}

IrOperand ir_var(const char *name, IrType type) {
    IrOperand op = ir_empty();
    op.kind = IR_OP_VAR;
    op.type = type;
    op.name = ir_strdup(name);
    return op;
}

IrOperand ir_const_int(long v, IrType type) {
    IrOperand op = ir_empty();
    op.kind = IR_OP_CONST;
    op.type = type;
    op.value.i = v;
    return op;
}

IrOperand ir_const_real(double v, IrType type) {
    IrOperand op = ir_empty();
    op.kind = IR_OP_CONST;
    op.type = type;
    op.value.d = v;
    return op;
}

IrOperand ir_label(const char *name) {
    IrOperand op = ir_empty();
    op.kind = IR_OP_LABEL;
    op.type = TYPE_UNKNOWN;
    op.name = ir_strdup(name);
    return op;
}

IrOperand ir_func_ref(const char *name) {
    IrOperand op = ir_empty();
    op.kind = IR_OP_FUNC;
    op.type = TYPE_UNKNOWN;
    op.name = ir_strdup(name);
    return op;
}

char *ir_new_label(IrProgram *p, const char *prefix) {
    char buf[64];
    int n = p->label_count++;
    snprintf(buf, sizeof buf, "%s%d", prefix ? prefix : "L", n);
    return ir_strdup(buf);
}

int ir_new_temp(IrProgram *p) {
    return p->temp_count++;
}

/* ===================== Emissão ===================== */

IrInstr *ir_emit(IrFunc *f, IrOpcode op) {
    IrInstr *ins = ir_calloc(1, sizeof *ins);
    ins->op = op;
    ins->type = TYPE_UNKNOWN;
    ins->res = ir_empty();
    ins->arg1 = ir_empty();
    ins->arg2 = ir_empty();
    ins->unop = 0;
    ins->binop = 0;
    ins->cast_type = TYPE_UNKNOWN;
    ins->print_string = NULL;
    ins->next = NULL;

    if (!f->first) {
        f->first = f->last = ins;
    } else {
        f->last->next = ins;
        f->last = ins;
    }
    return ins;
}

/* ===================== Impressão ===================== */

static void ir_print_operand(FILE *out, const IrOperand *op) {
    switch (op->kind) {
    case IR_OP_EMPTY:
        fputs("_", out);
        break;
    case IR_OP_TEMP:
        fprintf(out, "t%d", op->id);
        break;
    case IR_OP_VAR:
    case IR_OP_LABEL:
    case IR_OP_FUNC:
        fputs(op->name ? op->name : "?", out);
        break;
    case IR_OP_CONST:
        if (op->type == TYPE_FLOAT || op->type == TYPE_DOUBLE) {
            fprintf(out, "%g", op->value.d);
        } else if (op->type == TYPE_BOOLEAN) {
            fputs(op->value.i ? "true" : "false", out);
        } else if (op->type == TYPE_CHAR) {
            /* imprime como inteiro; a origem pode ser um char literal */
            fprintf(out, "%ld", op->value.i);
        } else {
            fprintf(out, "%ld", op->value.i);
        }
        break;
    }
}

static void ir_print_instr(FILE *out, const IrInstr *ins) {
    switch (ins->op) {
    case IR_NOP:
        fputs("    nop\n", out);
        break;
    case IR_ASSIGN:
        fputs("    ", out);
        ir_print_operand(out, &ins->res);
        fputs(" = ", out);
        ir_print_operand(out, &ins->arg1);
        fputc('\n', out);
        break;
    case IR_UNARY:
        fputs("    ", out);
        ir_print_operand(out, &ins->res);
        fprintf(out, " = %s ", op_name(ins->unop));
        ir_print_operand(out, &ins->arg1);
        fputc('\n', out);
        break;
    case IR_BINOP:
        fputs("    ", out);
        ir_print_operand(out, &ins->res);
        fputs(" = ", out);
        ir_print_operand(out, &ins->arg1);
        fprintf(out, " %s ", op_name(ins->binop));
        ir_print_operand(out, &ins->arg2);
        fputc('\n', out);
        break;
    case IR_CAST:
        fputs("    ", out);
        ir_print_operand(out, &ins->res);
        fprintf(out, " = (%s) ", type_name(ins->cast_type));
        ir_print_operand(out, &ins->arg1);
        fputc('\n', out);
        break;
    case IR_LABEL:
        ir_print_operand(out, &ins->res);
        fputs(":\n", out);
        break;
    case IR_GOTO:
        fputs("    goto ", out);
        ir_print_operand(out, &ins->res);
        fputc('\n', out);
        break;
    case IR_IF_GOTO:
        fputs("    if ", out);
        ir_print_operand(out, &ins->arg1);
        fputs(" goto ", out);
        ir_print_operand(out, &ins->res);
        fputc('\n', out);
        break;
    case IR_IF_FALSE:
        fputs("    ifFalse ", out);
        ir_print_operand(out, &ins->arg1);
        fputs(" goto ", out);
        ir_print_operand(out, &ins->res);
        fputc('\n', out);
        break;
    case IR_PARAM:
        fputs("    param ", out);
        ir_print_operand(out, &ins->arg1);
        fputc('\n', out);
        break;
    case IR_CALL:
        fputs("    ", out);
        if (ins->res.kind != IR_OP_EMPTY) {
            ir_print_operand(out, &ins->res);
            fputs(" = ", out);
        }
        fputs("call ", out);
        ir_print_operand(out, &ins->arg1);
        fputs(", ", out);
        ir_print_operand(out, &ins->arg2);
        fputc('\n', out);
        break;
    case IR_RETURN:
        fputs("    return", out);
        if (ins->arg1.kind != IR_OP_EMPTY) {
            fputc(' ', out);
            ir_print_operand(out, &ins->arg1);
        }
        fputc('\n', out);
        break;
    case IR_PRINT:
        fputs("    print ", out);
        if (ins->print_string) {
            fprintf(out, "\"%s\"", ins->print_string);
        } else {
            ir_print_operand(out, &ins->arg1);
        }
        fputc('\n', out);
        break;
    }
}

void ir_print_program(FILE *out, const IrProgram *p) {
    if (!out) out = stdout;
    if (!p) {
        fprintf(out, "(IR nula)\n");
        return;
    }

    fprintf(out, "=== IR (TAC) ===\n");

    /* Globais */
    for (IrGlobal *g = p->globals; g; g = g->next) {
        fprintf(out, "global %s: %s", g->name, ir_type_name(g->type));
        if (g->has_init) {
            fputs(" = ", out);
            ir_print_operand(out, &g->init);
        }
        fputc('\n', out);
    }
    if (p->globals) fputc('\n', out);

    /* Funções */
    for (IrFunc *f = p->funcs; f; f = f->next) {
        fprintf(out, "func %s(", f->name ? f->name : "?");
        for (IrParam *pr = f->params; pr; pr = pr->next) {
            fprintf(out, "%s: %s", pr->name, ir_type_name(pr->type));
            if (pr->next) fputs(", ", out);
        }
        fprintf(out, ") -> %s\n", ir_type_name(f->ret_type));
        for (IrInstr *ins = f->first; ins; ins = ins->next) {
            ir_print_instr(out, ins);
        }
        fputc('\n', out);
    }
}
