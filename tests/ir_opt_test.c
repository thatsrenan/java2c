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

    func->temp_count = 2;
    emit_binop(func, 0, OP_ADD, ir_const_int(2, TYPE_INT),
               ir_const_int(3, TYPE_INT), TYPE_INT);
    emit_binop(func, 1, OP_MULT, ir_temp(0, TYPE_INT),
               ir_const_int(4, TYPE_INT), TYPE_INT);
    IrInstr *store = emit_assign(func, ir_var("x", TYPE_INT),
                                 ir_temp(1, TYPE_INT), TYPE_INT);
    IrInstr *print = ir_emit(func, IR_PRINT);
    print->arg1 = ir_var("x", TYPE_INT);

    ir_optimize(program);

    CHECK(store->op == IR_ASSIGN);
    check_int_constant(&store->arg1, 20);
    check_int_constant(&print->arg1, 20);
    CHECK(func->temp_count == 0);

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
    branch->arg1 = ir_var("cond", TYPE_BOOLEAN);
    branch->res = ir_label("L1");
    IrInstr *after_branch = ir_emit(func, IR_PRINT);
    after_branch->arg1 = ir_var("x", TYPE_INT);

    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(10, TYPE_INT), TYPE_INT);
    ir_emit(func, IR_GOTO)->res = ir_label("L2");
    ir_emit(func, IR_PRINT)->arg1 = ir_var("x", TYPE_INT);
    IrInstr *label2 = ir_emit(func, IR_LABEL);
    label2->res = ir_label("L2");
    emit_assign(func, ir_var("x", TYPE_INT),
                ir_const_int(9, TYPE_INT), TYPE_INT);
    IrInstr *call = ir_emit(func, IR_CALL);
    call->arg1 = ir_func_ref("may_change_globals");
    call->arg2 = ir_const_int(0, TYPE_INT);
    IrInstr *after_call = ir_emit(func, IR_PRINT);
    after_call->arg1 = ir_var("x", TYPE_INT);

    ir_optimize(program);

    check_int_constant(&after_label->arg1, 7);
    CHECK(branch->arg1.kind == IR_OP_VAR);
    check_int_constant(&after_branch->arg1, 8);
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
    emit_binop(func, 0, OP_ADD, ir_const_int(INT32_MAX, TYPE_INT),
               ir_const_int(1, TYPE_INT), TYPE_INT);
    emit_binop(func, 1, OP_DIV, ir_const_int(INT32_MIN, TYPE_INT),
               ir_const_int(-1, TYPE_INT), TYPE_INT);
    emit_binop(func, 2, OP_MOD, ir_const_int(INT32_MIN, TYPE_INT),
               ir_const_int(-1, TYPE_INT), TYPE_INT);
    {
        IrInstr *cast = ir_emit(func, IR_CAST);
        cast->type = TYPE_CHAR;
        cast->cast_type = TYPE_CHAR;
        cast->res = ir_temp(3, TYPE_CHAR);
        cast->arg1 = ir_const_int(65536, TYPE_INT);
    }
    IrInstr *print_add = ir_emit(func, IR_PRINT);
    print_add->arg1 = ir_temp(0, TYPE_INT);
    IrInstr *print_div = ir_emit(func, IR_PRINT);
    print_div->arg1 = ir_temp(1, TYPE_INT);
    IrInstr *print_mod = ir_emit(func, IR_PRINT);
    print_mod->arg1 = ir_temp(2, TYPE_INT);
    IrInstr *print_cast = ir_emit(func, IR_PRINT);
    print_cast->arg1 = ir_temp(3, TYPE_CHAR);

    ir_optimize(program);

    check_int_constant(&print_add->arg1, INT32_MIN);
    check_int_constant(&print_div->arg1, INT32_MIN);
    check_int_constant(&print_mod->arg1, 0);
    CHECK(print_cast->arg1.kind == IR_OP_CONST);
    CHECK(print_cast->arg1.type == TYPE_CHAR);
    CHECK(print_cast->arg1.value.i == 0);

    ir_program_free(program);
}

static void test_dead_temporary_chain_is_removed(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    func->temp_count = 2;
    emit_binop(func, 0, OP_ADD, ir_const_int(1, TYPE_INT),
               ir_const_int(2, TYPE_INT), TYPE_INT);
    emit_binop(func, 1, OP_MULT, ir_temp(0, TYPE_INT),
               ir_const_int(3, TYPE_INT), TYPE_INT);

    ir_optimize(program);

    CHECK(func->first == NULL);
    CHECK(func->last == NULL);
    CHECK(func->temp_count == 0);

    ir_program_free(program);
}

static void test_constant_branch_removes_unreachable_block(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    {
        IrInstr *branch = ir_emit(func, IR_IF_FALSE);
        branch->arg1 = ir_const_int(1, TYPE_BOOLEAN);
        branch->res = ir_label("Ldead");
    }
    IrInstr *live_print = ir_emit(func, IR_PRINT);
    live_print->arg1 = ir_const_int(1, TYPE_INT);
    ir_emit(func, IR_GOTO)->res = ir_label("Lend");
    ir_emit(func, IR_LABEL)->res = ir_label("Ldead");
    ir_emit(func, IR_PRINT)->arg1 = ir_const_int(2, TYPE_INT);
    IrInstr *end_label = ir_emit(func, IR_LABEL);
    end_label->res = ir_label("Lend");
    IrInstr *ret = ir_emit(func, IR_RETURN);

    ir_optimize(program);

    CHECK(func->first == live_print);
    CHECK(live_print->next == ret);
    CHECK(func->last == ret);

    ir_program_free(program);
}

static void test_local_cse_and_invalidation(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    ir_func_add_local(func, "x", TYPE_INT);
    ir_func_add_local(func, "y", TYPE_INT);
    ir_func_add_local(func, "z", TYPE_INT);
    ir_func_add_local(func, "w", TYPE_INT);
    func->temp_count = 4;

    IrInstr *first = emit_binop(func, 0, OP_ADD, ir_var("x", TYPE_INT),
                                ir_var("y", TYPE_INT), TYPE_INT);
    IrInstr *duplicate = emit_binop(func, 1, OP_ADD, ir_var("x", TYPE_INT),
                                    ir_var("y", TYPE_INT), TYPE_INT);
    IrInstr *print_duplicate = ir_emit(func, IR_PRINT);
    print_duplicate->arg1 = ir_temp(1, TYPE_INT);
    emit_assign(func, ir_var("y", TYPE_INT), ir_var("z", TYPE_INT), TYPE_INT);
    IrInstr *after_redefinition = emit_binop(
        func, 2, OP_ADD, ir_var("x", TYPE_INT), ir_var("y", TYPE_INT), TYPE_INT);
    IrInstr *print_after_redefinition = ir_emit(func, IR_PRINT);
    print_after_redefinition->arg1 = ir_temp(2, TYPE_INT);
    emit_assign(func, ir_var("x", TYPE_INT), ir_var("z", TYPE_INT), TYPE_INT);
    IrInstr *after_x_change = emit_binop(
        func, 3, OP_ADD, ir_var("x", TYPE_INT), ir_var("y", TYPE_INT), TYPE_INT);
    IrInstr *print = ir_emit(func, IR_PRINT);
    print->arg1 = ir_temp(3, TYPE_INT);

    ir_optimize(program);

    CHECK(first->op == IR_BINOP);
    CHECK(duplicate->op == IR_ASSIGN);
    CHECK(duplicate->arg1.kind == IR_OP_TEMP);
    CHECK(duplicate->arg1.id == 0);
    CHECK(after_redefinition->op == IR_BINOP);
    CHECK(after_x_change->op == IR_BINOP);

    ir_program_free(program);
}

static void test_cse_does_not_cross_control_flow_or_calls(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    ir_func_add_local(func, "x", TYPE_INT);
    ir_func_add_local(func, "y", TYPE_INT);
    ir_func_add_local(func, "cond", TYPE_BOOLEAN);
    func->temp_count = 4;

    emit_binop(func, 0, OP_ADD, ir_var("x", TYPE_INT),
               ir_var("y", TYPE_INT), TYPE_INT);
    IrInstr *branch = ir_emit(func, IR_IF_FALSE);
    branch->arg1 = ir_var("cond", TYPE_BOOLEAN);
    branch->res = ir_label("Lelse");
    IrInstr *in_then = emit_binop(func, 1, OP_ADD, ir_var("x", TYPE_INT),
                                  ir_var("y", TYPE_INT), TYPE_INT);
    IrInstr *skip_else = ir_emit(func, IR_GOTO);
    skip_else->res = ir_label("Ljoin");
    ir_emit(func, IR_LABEL)->res = ir_label("Lelse");
    IrInstr *in_else = emit_binop(func, 2, OP_ADD, ir_var("x", TYPE_INT),
                                  ir_var("y", TYPE_INT), TYPE_INT);
    IrInstr *call = ir_emit(func, IR_CALL);
    call->arg1 = ir_func_ref("may_change_globals");
    call->arg2 = ir_const_int(0, TYPE_INT);
    IrInstr *after_call = emit_binop(func, 3, OP_ADD, ir_var("x", TYPE_INT),
                                     ir_var("y", TYPE_INT), TYPE_INT);
    ir_emit(func, IR_LABEL)->res = ir_label("Ljoin");
    ir_emit(func, IR_PRINT)->arg1 = ir_temp(1, TYPE_INT);
    ir_emit(func, IR_PRINT)->arg1 = ir_temp(2, TYPE_INT);
    ir_emit(func, IR_PRINT)->arg1 = ir_temp(3, TYPE_INT);

    ir_optimize(program);

    CHECK(in_then->op == IR_BINOP);
    CHECK(in_else->op == IR_BINOP);
    CHECK(after_call->op == IR_BINOP);

    ir_program_free(program);
}

static void test_constant_dataflow_across_join_and_loop(void) {
    IrProgram *program = ir_program_new();
    IrFunc *func = ir_program_add_func(program, "main", TYPE_VOID);
    ir_func_add_local(func, "x", TYPE_INT);
    ir_func_add_local(func, "cond", TYPE_BOOLEAN);

    emit_assign(func, ir_var("x", TYPE_INT), ir_const_int(5, TYPE_INT), TYPE_INT);
    IrInstr *branch = ir_emit(func, IR_IF_FALSE);
    branch->arg1 = ir_var("cond", TYPE_BOOLEAN);
    branch->res = ir_label("Lelse");
    emit_assign(func, ir_var("x", TYPE_INT), ir_const_int(7, TYPE_INT), TYPE_INT);
    IrInstr *skip_else = ir_emit(func, IR_GOTO);
    skip_else->res = ir_label("Ljoin");
    IrInstr *else_label = ir_emit(func, IR_LABEL);
    else_label->res = ir_label("Lelse");
    emit_assign(func, ir_var("x", TYPE_INT), ir_const_int(7, TYPE_INT), TYPE_INT);
    IrInstr *join_label = ir_emit(func, IR_LABEL);
    join_label->res = ir_label("Ljoin");
    IrInstr *join_print = ir_emit(func, IR_PRINT);
    join_print->arg1 = ir_var("x", TYPE_INT);

    emit_assign(func, ir_var("x", TYPE_INT), ir_const_int(9, TYPE_INT), TYPE_INT);
    IrInstr *loop_label = ir_emit(func, IR_LABEL);
    loop_label->res = ir_label("Lloop");
    IrInstr *loop_print = ir_emit(func, IR_PRINT);
    loop_print->arg1 = ir_var("x", TYPE_INT);
    IrInstr *loop_branch = ir_emit(func, IR_IF_FALSE);
    loop_branch->arg1 = ir_var("cond", TYPE_BOOLEAN);
    loop_branch->res = ir_label("Lexit");
    emit_assign(func, ir_var("x", TYPE_INT), ir_const_int(10, TYPE_INT), TYPE_INT);
    IrInstr *back_edge = ir_emit(func, IR_GOTO);
    back_edge->res = ir_label("Lloop");
    IrInstr *exit_label = ir_emit(func, IR_LABEL);
    exit_label->res = ir_label("Lexit");
    IrInstr *exit_print = ir_emit(func, IR_PRINT);
    exit_print->arg1 = ir_var("x", TYPE_INT);

    ir_optimize(program);

    CHECK(join_print->arg1.kind == IR_OP_CONST);
    CHECK(join_print->arg1.value.i == 7);
    CHECK(loop_print->arg1.kind == IR_OP_VAR);
    CHECK(exit_print->arg1.kind == IR_OP_VAR);

    ir_program_free(program);
}

int main(void) {
    test_folding_and_propagation();
    test_reassignment_invalidates_previous_value();
    test_control_flow_and_calls_clear_facts();
    test_division_by_zero_is_not_folded();
    test_integer_wraparound_and_cast_folding();
    test_dead_temporary_chain_is_removed();
    test_constant_branch_removes_unreachable_block();
    test_local_cse_and_invalidation();
    test_cse_does_not_cross_control_flow_or_calls();
    test_constant_dataflow_across_join_and_loop();

    if (failures) {
        fprintf(stderr, "%d de %d verificacoes falharam.\n", failures, checks);
        return 1;
    }
    printf("%d verificacoes passaram.\n", checks);
    return 0;
}
