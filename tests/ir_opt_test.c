#include "ir_opt.h"
#include "const_eval.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
static int checks;

#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        fprintf(stderr, "Falha na linha %d: %s\n", __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static IrInstr *emit_assign(IrFunc *func, IrOperand dst, IrOperand src,
                            IrType type) {
    IrInstr *ins = ir_emit(func, IR_ASSIGN);
    ins->type = type;
    ins->res = dst;
    ins->arg1 = src;
    return ins;
}

static IrInstr *emit_binop(IrFunc *func, int id, IrBinOp op,
                           IrOperand left, IrOperand right, IrType type) {
    IrInstr *ins = ir_emit(func, IR_BINOP);
    ins->type = type;
    ins->binop = op;
    ins->res = ir_temp(id, type);
    ins->arg1 = left;
    ins->arg2 = right;
    return ins;
}

static void check_int_constant(const IrOperand *op, long expected) {
    CHECK(op->kind == IR_OP_CONST);
    CHECK(op->type == TYPE_INT || op->type == TYPE_CHAR ||
          op->type == TYPE_BOOLEAN);
    CHECK(op->value.i == expected);
}

static void test_folding_and_propagation(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    ir_func_add_local(func, "x", TYPE_INT);

    IrInstr *multiply = emit_binop(func, 0, OP_ADD,
                                   ir_const_int(2, TYPE_INT),
                                   ir_const_int(3, TYPE_INT), TYPE_INT);
    IrInstr *combine = emit_binop(func, 1, OP_MULT,
                                  ir_temp(0, TYPE_INT),
                                  ir_const_int(4, TYPE_INT), TYPE_INT);
    IrInstr *store = emit_assign(func, ir_var("x", TYPE_INT),
                                 ir_temp(1, TYPE_INT), TYPE_INT);
    IrInstr *print = ir_emit(func, IR_PRINT);
    print->arg1 = ir_var("x", TYPE_INT);

    ir_optimize(program);

    CHECK(multiply->op == IR_ASSIGN);
    check_int_constant(&multiply->arg1, 5);
    CHECK(combine->op == IR_ASSIGN);
    check_int_constant(&combine->arg1, 20);
    CHECK(store->op == IR_ASSIGN);
    check_int_constant(&store->arg1, 20);
    check_int_constant(&print->arg1, 20);

    ir_program_free(program);
}

static void test_reassignment_invalidates_previous_value(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);

    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(1, TYPE_INT), TYPE_INT);
    IrInstr *first_print = ir_emit(func, IR_PRINT);
    first_print->arg1 = ir_var("x", TYPE_INT);
    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(2, TYPE_INT), TYPE_INT);
    IrInstr *second_print = ir_emit(func, IR_PRINT);
    second_print->arg1 = ir_var("x", TYPE_INT);

    ir_optimize(program);

    check_int_constant(&first_print->arg1, 1);
    check_int_constant(&second_print->arg1, 2);

    ir_program_free(program);
}

static void test_control_flow_and_calls_clear_facts(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);

    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(7, TYPE_INT), TYPE_INT);
    IrInstr *label = ir_emit(func, IR_LABEL);
    label->res = ir_label("L0");
    IrInstr *after_label = ir_emit(func, IR_PRINT);
    after_label->arg1 = ir_var("x", TYPE_INT);

    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(8, TYPE_INT), TYPE_INT);
    IrInstr *branch = ir_emit(func, IR_IF_FALSE);
    branch->arg1 = ir_const_int(0, TYPE_BOOLEAN);
    branch->res = ir_label("L1");
    IrInstr *after_branch = ir_emit(func, IR_PRINT);
    after_branch->arg1 = ir_var("x", TYPE_INT);

    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(10, TYPE_INT), TYPE_INT);
    IrInstr *jump = ir_emit(func, IR_GOTO);
    jump->res = ir_label("L2");
    IrInstr *after_jump = ir_emit(func, IR_PRINT);
    after_jump->arg1 = ir_var("x", TYPE_INT);

    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(9, TYPE_INT), TYPE_INT);
    IrInstr *call = ir_emit(func, IR_CALL);
    call->arg1 = ir_func_ref("may_change_globals");
    call->arg2 = ir_const_int(0, TYPE_INT);
    IrInstr *after_call = ir_emit(func, IR_PRINT);
    after_call->arg1 = ir_var("x", TYPE_INT);

    ir_optimize(program);

    CHECK(after_label->arg1.kind == IR_OP_VAR);
    CHECK(branch->arg1.kind == IR_OP_CONST);
    CHECK(after_branch->arg1.kind == IR_OP_VAR);
    CHECK(after_jump->arg1.kind == IR_OP_VAR);
    CHECK(after_call->arg1.kind == IR_OP_VAR);

    ir_program_free(program);
}

static void test_division_by_zero_is_not_folded(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    IrInstr *division = emit_binop(func, 0, OP_DIV,
                                   ir_const_int(1, TYPE_INT),
                                   ir_const_int(0, TYPE_INT), TYPE_INT);
    IrInstr *print = ir_emit(func, IR_PRINT);
    print->arg1 = ir_temp(0, TYPE_INT);

    ir_optimize(program);

    CHECK(division->op == IR_BINOP);
    CHECK(print->arg1.kind == IR_OP_TEMP);

    ir_program_free(program);
}

static void test_integer_wraparound_and_cast_folding(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    IrInstr *addition = emit_binop(func, 0, OP_ADD,
                                   ir_const_int(INT32_MAX, TYPE_INT),
                                   ir_const_int(1, TYPE_INT), TYPE_INT);
    IrInstr *division = emit_binop(func, 1, OP_DIV,
                                   ir_const_int(INT32_MIN, TYPE_INT),
                                   ir_const_int(-1, TYPE_INT), TYPE_INT);
    IrInstr *remainder = emit_binop(func, 2, OP_MOD,
                                    ir_const_int(INT32_MIN, TYPE_INT),
                                    ir_const_int(-1, TYPE_INT), TYPE_INT);
    IrInstr *cast = ir_emit(func, IR_CAST);
    cast->type = TYPE_CHAR;
    cast->cast_type = TYPE_CHAR;
    cast->res = ir_temp(3, TYPE_CHAR);
    cast->arg1 = ir_const_int(65536, TYPE_INT);

    ir_optimize(program);

    CHECK(addition->op == IR_ASSIGN);
    check_int_constant(&addition->arg1, INT32_MIN);
    CHECK(division->op == IR_ASSIGN);
    check_int_constant(&division->arg1, INT32_MIN);
    CHECK(remainder->op == IR_ASSIGN);
    check_int_constant(&remainder->arg1, 0);
    CHECK(cast->op == IR_ASSIGN);
    CHECK(cast->arg1.kind == IR_OP_CONST);
    CHECK(cast->arg1.type == TYPE_CHAR);
    CHECK(cast->arg1.value.i == 0);

    ir_program_free(program);
}

int main(void) {
    test_folding_and_propagation();
    test_reassignment_invalidates_previous_value();
    test_control_flow_and_calls_clear_facts();
    test_division_by_zero_is_not_folded();
    test_integer_wraparound_and_cast_folding();

    if (failures) {
        fprintf(stderr, "%d de %d verificacoes falharam.\n", failures, checks);
        return 1;
    }
    printf("%d verificacoes passaram.\n", checks);
    return 0;
}
