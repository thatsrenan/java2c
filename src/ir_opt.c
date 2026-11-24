#include "ir_opt.h"

#include "const_eval.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ConstBinding {
    IrOperandKind kind;
    int temp_id;
    char *var_name;
    ConstVal value;
    struct ConstBinding *next;
} ConstBinding;

static void *opt_calloc(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (!p) {
        fprintf(stderr, "ERRO: sem memoria ao otimizar a RI.\n");
        exit(1);
    }
    return p;
}

static char *opt_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = opt_calloc(len, sizeof *copy);
    memcpy(copy, s, len);
    return copy;
}

static int is_real(IrType type) {
    return type == TYPE_FLOAT || type == TYPE_DOUBLE;
}

static ConstVal operand_value(const IrOperand *op) {
    ConstVal value;
    value.type = op->type;
    value.i = op->value.i;
    value.d = op->value.d;
    return value;
}

static IrOperand value_operand(ConstVal value) {
    if (is_real(value.type)) return ir_const_real(value.d, value.type);
    return ir_const_int(value.i, value.type);
}

static int binding_matches(const ConstBinding *binding, const IrOperand *key) {
    if (binding->kind != key->kind) return 0;
    if (key->kind == IR_OP_TEMP) return binding->temp_id == key->id;
    return key->kind == IR_OP_VAR && binding->var_name && key->name &&
           strcmp(binding->var_name, key->name) == 0;
}

static ConstBinding *binding_find(ConstBinding *bindings, const IrOperand *key) {
    if (key->kind != IR_OP_TEMP && key->kind != IR_OP_VAR) return NULL;
    for (ConstBinding *b = bindings; b; b = b->next)
        if (binding_matches(b, key)) return b;
    return NULL;
}

static void binding_remove(ConstBinding **bindings, const IrOperand *key) {
    ConstBinding **link = bindings;
    while (*link) {
        ConstBinding *b = *link;
        if (binding_matches(b, key)) {
            *link = b->next;
            free(b->var_name);
            free(b);
            return;
        }
        link = &b->next;
    }
}

static void binding_set(ConstBinding **bindings, const IrOperand *key,
                        ConstVal value) {
    if (key->kind != IR_OP_TEMP && key->kind != IR_OP_VAR) return;
    binding_remove(bindings, key);

    ConstBinding *b = opt_calloc(1, sizeof *b);
    b->kind = key->kind;
    b->temp_id = key->id;
    if (key->kind == IR_OP_VAR) b->var_name = opt_strdup(key->name);
    b->value = value;
    b->next = *bindings;
    *bindings = b;
}

static void bindings_clear(ConstBinding **bindings) {
    while (*bindings) {
        ConstBinding *next = (*bindings)->next;
        free((*bindings)->var_name);
        free(*bindings);
        *bindings = next;
    }
}

static void propagate_operand(ConstBinding *bindings, IrOperand *op) {
    ConstBinding *b = binding_find(bindings, op);
    if (!b) return;
    ir_operand_release(op);
    *op = value_operand(b->value);
}

static ConstVal value_for_destination(ConstVal value, const IrInstr *ins) {
    IrType type = ins->type;
    if (type == TYPE_UNKNOWN) type = ins->res.type;
    if (type != TYPE_UNKNOWN && type != TYPE_VOID)
        return const_convert(value, type);
    return value;
}

static void record_assignment(ConstBinding **bindings, IrInstr *ins) {
    if (ins->res.kind != IR_OP_TEMP && ins->res.kind != IR_OP_VAR) return;

    binding_remove(bindings, &ins->res);
    if (ins->arg1.kind == IR_OP_CONST) {
        ConstVal value = value_for_destination(operand_value(&ins->arg1), ins);
        binding_set(bindings, &ins->res, value);
    }
}

static void replace_with_assignment(IrInstr *ins, ConstVal value) {
    ir_operand_release(&ins->arg1);
    ir_operand_release(&ins->arg2);
    ins->arg1 = value_operand(value);
    ins->arg2 = ir_empty();
    ins->op = IR_ASSIGN;
    ins->unop = 0;
    ins->binop = 0;
    ins->cast_type = TYPE_UNKNOWN;
}

static void optimize_function(IrFunc *func) {
    ConstBinding *bindings = NULL;

    for (IrInstr *ins = func->first; ins; ins = ins->next) {
        ConstVal result;
        int folded = 0;

        switch (ins->op) {
        case IR_LABEL:
            bindings_clear(&bindings);
            break;

        case IR_ASSIGN:
            propagate_operand(bindings, &ins->arg1);
            record_assignment(&bindings, ins);
            break;

        case IR_UNARY:
            propagate_operand(bindings, &ins->arg1);
            if (ins->arg1.kind == IR_OP_CONST)
                folded = const_eval_unary(ins->unop, operand_value(&ins->arg1), &result);
            if (folded) {
                result = value_for_destination(result, ins);
                replace_with_assignment(ins, result);
                record_assignment(&bindings, ins);
            } else {
                binding_remove(&bindings, &ins->res);
            }
            break;

        case IR_BINOP:
            propagate_operand(bindings, &ins->arg1);
            propagate_operand(bindings, &ins->arg2);
            if (ins->arg1.kind == IR_OP_CONST && ins->arg2.kind == IR_OP_CONST)
                folded = const_eval_binary(ins->binop, operand_value(&ins->arg1),
                                           operand_value(&ins->arg2), &result);
            if (folded) {
                result = value_for_destination(result, ins);
                replace_with_assignment(ins, result);
                record_assignment(&bindings, ins);
            } else {
                binding_remove(&bindings, &ins->res);
            }
            break;

        case IR_CAST:
            propagate_operand(bindings, &ins->arg1);
            if (ins->arg1.kind == IR_OP_CONST) {
                result = const_convert(operand_value(&ins->arg1), ins->cast_type);
                result = value_for_destination(result, ins);
                replace_with_assignment(ins, result);
                record_assignment(&bindings, ins);
            } else {
                binding_remove(&bindings, &ins->res);
            }
            break;

        case IR_IF_GOTO:
        case IR_IF_FALSE:
            propagate_operand(bindings, &ins->arg1);
            bindings_clear(&bindings);
            break;

        case IR_GOTO:
            bindings_clear(&bindings);
            break;

        case IR_CALL:
            bindings_clear(&bindings);
            break;

        case IR_PARAM:
        case IR_PRINT:
        case IR_RETURN:
            propagate_operand(bindings, &ins->arg1);
            break;

        case IR_NOP:
            break;
        }
    }

    bindings_clear(&bindings);
}

void ir_optimize(IrProgram *program) {
    if (!program) return;
    for (IrFunc *func = program->funcs; func; func = func->next)
        optimize_function(func);
}
