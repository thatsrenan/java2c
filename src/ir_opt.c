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

typedef struct {
    IrInstr *first;
    IrInstr *last;
    int successor[2];
    int successor_count;
    int reachable;
    int facts_valid;
    ConstBinding *in;
    ConstBinding *out;
} BasicBlock;

typedef struct {
    BasicBlock *blocks;
    size_t count;
} ControlFlowGraph;

typedef struct CseEntry {
    IrOpcode op;
    IrType type;
    IrUnOp unop;
    IrBinOp binop;
    IrType cast_type;
    IrOperand arg1;
    IrOperand arg2;
    IrOperand result;
    struct CseEntry *next;
} CseEntry;

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

static IrOperand operand_copy(const IrOperand *op) {
    switch (op->kind) {
    case IR_OP_VAR: return ir_var(op->name, op->type);
    case IR_OP_LABEL: return ir_label(op->name);
    case IR_OP_FUNC: return ir_func_ref(op->name);
    default: return *op;
    }
}

static int operands_equal(const IrOperand *a, const IrOperand *b) {
    if (a->kind != b->kind || a->type != b->type) return 0;
    switch (a->kind) {
    case IR_OP_EMPTY:
        return 1;
    case IR_OP_TEMP:
        return a->id == b->id;
    case IR_OP_VAR:
    case IR_OP_LABEL:
    case IR_OP_FUNC:
        return a->name && b->name && strcmp(a->name, b->name) == 0;
    case IR_OP_CONST:
        if (is_real(a->type))
            return memcmp(&a->value.d, &b->value.d, sizeof a->value.d) == 0;
        return a->value.i == b->value.i;
    }
    return 0;
}

static int const_values_equal(ConstVal a, ConstVal b) {
    if (a.type != b.type) return 0;
    if (is_real(a.type)) return memcmp(&a.d, &b.d, sizeof a.d) == 0;
    return a.i == b.i;
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

static ConstBinding *bindings_clone(const ConstBinding *source) {
    ConstBinding *copy = NULL;
    ConstBinding **tail = &copy;
    for (const ConstBinding *b = source; b; b = b->next) {
        ConstBinding *item = opt_calloc(1, sizeof *item);
        item->kind = b->kind;
        item->temp_id = b->temp_id;
        item->var_name = b->var_name ? opt_strdup(b->var_name) : NULL;
        item->value = b->value;
        *tail = item;
        tail = &item->next;
    }
    return copy;
}

static int bindings_equal(const ConstBinding *a, const ConstBinding *b) {
    for (const ConstBinding *it = a; it; it = it->next) {
        const ConstBinding *other = NULL;
        for (const ConstBinding *candidate = b; candidate; candidate = candidate->next)
            if (binding_matches(candidate, &(IrOperand){
                    .kind = it->kind, .id = it->temp_id, .name = it->var_name })) {
                other = candidate;
                break;
            }
        if (!other || !const_values_equal(it->value, other->value)) return 0;
    }
    for (const ConstBinding *it = b; it; it = it->next) {
        const ConstBinding *other = NULL;
        for (const ConstBinding *candidate = a; candidate; candidate = candidate->next)
            if (binding_matches(candidate, &(IrOperand){
                    .kind = it->kind, .id = it->temp_id, .name = it->var_name })) {
                other = candidate;
                break;
            }
        if (!other) return 0;
    }
    return 1;
}

static void bindings_intersect(ConstBinding **target, const ConstBinding *other) {
    ConstBinding **link = target;
    while (*link) {
        ConstBinding *b = *link;
        const ConstBinding *match = NULL;
        for (const ConstBinding *candidate = other; candidate; candidate = candidate->next) {
            IrOperand key = ir_empty();
            key.kind = b->kind;
            key.id = b->temp_id;
            key.name = b->var_name;
            if (binding_matches(candidate, &key)) {
                match = candidate;
                break;
            }
        }
        if (!match || !const_values_equal(b->value, match->value)) {
            *link = b->next;
            free(b->var_name);
            free(b);
        } else {
            link = &b->next;
        }
    }
}

static void propagate_operand(ConstBinding *bindings, IrOperand *op) {
    ConstBinding *b = binding_find(bindings, op);
    if (!b) return;
    ir_operand_release(op);
    *op = value_operand(b->value);
}

static IrOperand resolved_operand(ConstBinding *bindings, const IrOperand *op) {
    ConstBinding *b = binding_find(bindings, op);
    return b ? value_operand(b->value) : operand_copy(op);
}

static ConstVal value_for_destination(ConstVal value, const IrInstr *ins) {
    IrType type = ins->type;
    if (type == TYPE_UNKNOWN) type = ins->res.type;
    if (type != TYPE_UNKNOWN && type != TYPE_VOID)
        return const_convert(value, type);
    return value;
}

static void transfer_instruction(ConstBinding **facts, IrInstr *ins, int rewrite) {
    ConstVal result;
    int folded = 0;

    switch (ins->op) {
    case IR_ASSIGN: {
        if (rewrite) propagate_operand(*facts, &ins->arg1);
        IrOperand value = rewrite ? operand_copy(&ins->arg1)
                                  : resolved_operand(*facts, &ins->arg1);
        binding_remove(facts, &ins->res);
        if (value.kind == IR_OP_CONST &&
            (ins->res.kind == IR_OP_TEMP || ins->res.kind == IR_OP_VAR)) {
            ConstVal c = value_for_destination(operand_value(&value), ins);
            binding_set(facts, &ins->res, c);
        }
        ir_operand_release(&value);
        break;
    }

    case IR_UNARY: {
        IrOperand left = rewrite ? (propagate_operand(*facts, &ins->arg1),
                                    operand_copy(&ins->arg1))
                                : resolved_operand(*facts, &ins->arg1);
        if (left.kind == IR_OP_CONST)
            folded = const_eval_unary(ins->unop, operand_value(&left), &result);
        if (rewrite && folded) {
            result = value_for_destination(result, ins);
            ir_operand_release(&ins->arg1);
            ins->arg1 = value_operand(result);
            ir_operand_release(&ins->arg2);
            ins->arg2 = ir_empty();
            ins->op = IR_ASSIGN;
            ins->unop = 0;
            ins->cast_type = TYPE_UNKNOWN;
            binding_remove(facts, &ins->res);
            binding_set(facts, &ins->res, result);
        } else {
            binding_remove(facts, &ins->res);
            if (folded)
                binding_set(facts, &ins->res, value_for_destination(result, ins));
        }
        ir_operand_release(&left);
        break;
    }

    case IR_BINOP: {
        IrOperand left = rewrite ? (propagate_operand(*facts, &ins->arg1),
                                    operand_copy(&ins->arg1))
                                : resolved_operand(*facts, &ins->arg1);
        IrOperand right = rewrite ? (propagate_operand(*facts, &ins->arg2),
                                     operand_copy(&ins->arg2))
                                 : resolved_operand(*facts, &ins->arg2);
        if (left.kind == IR_OP_CONST && right.kind == IR_OP_CONST)
            folded = const_eval_binary(ins->binop, operand_value(&left),
                                       operand_value(&right), &result);
        if (rewrite && folded) {
            result = value_for_destination(result, ins);
            ir_operand_release(&ins->arg1);
            ins->arg1 = value_operand(result);
            ir_operand_release(&ins->arg2);
            ins->arg2 = ir_empty();
            ins->op = IR_ASSIGN;
            ins->binop = 0;
            ins->cast_type = TYPE_UNKNOWN;
            binding_remove(facts, &ins->res);
            binding_set(facts, &ins->res, result);
        } else {
            binding_remove(facts, &ins->res);
            if (folded)
                binding_set(facts, &ins->res, value_for_destination(result, ins));
        }
        ir_operand_release(&left);
        ir_operand_release(&right);
        break;
    }

    case IR_CAST: {
        IrOperand value = rewrite ? (propagate_operand(*facts, &ins->arg1),
                                     operand_copy(&ins->arg1))
                                 : resolved_operand(*facts, &ins->arg1);
        if (value.kind == IR_OP_CONST) {
            result = const_convert(operand_value(&value), ins->cast_type);
            result = value_for_destination(result, ins);
            if (rewrite) {
                ir_operand_release(&ins->arg1);
                ins->arg1 = value_operand(result);
                ir_operand_release(&ins->arg2);
                ins->arg2 = ir_empty();
                ins->op = IR_ASSIGN;
                ins->cast_type = TYPE_UNKNOWN;
            }
            binding_remove(facts, &ins->res);
            binding_set(facts, &ins->res, result);
        } else {
            binding_remove(facts, &ins->res);
        }
        ir_operand_release(&value);
        break;
    }

    case IR_IF_GOTO:
    case IR_IF_FALSE:
    case IR_PARAM:
    case IR_PRINT:
    case IR_RETURN:
        if (rewrite) propagate_operand(*facts, &ins->arg1);
        break;

    case IR_CALL:
        for (ConstBinding *b = *facts; b;) {
            ConstBinding *next = b->next;
            if (b->kind == IR_OP_VAR) {
                IrOperand key = ir_var(b->var_name, TYPE_UNKNOWN);
                binding_remove(facts, &key);
                ir_operand_release(&key);
            }
            b = next;
        }
        if (ins->res.kind == IR_OP_TEMP) binding_remove(facts, &ins->res);
        break;

    case IR_LABEL:
    case IR_GOTO:
    case IR_NOP:
        break;
    }
}

static size_t block_index_for_label(const ControlFlowGraph *cfg,
                                    const char *label) {
    if (!label) return cfg->count;
    for (size_t i = 0; i < cfg->count; i++) {
        IrInstr *first = cfg->blocks[i].first;
        if (first && first->op == IR_LABEL && first->res.name &&
            strcmp(first->res.name, label) == 0)
            return i;
    }
    return cfg->count;
}

static void cfg_free(ControlFlowGraph *cfg) {
    if (!cfg) return;
    for (size_t i = 0; i < cfg->count; i++) {
        bindings_clear(&cfg->blocks[i].in);
        bindings_clear(&cfg->blocks[i].out);
    }
    free(cfg->blocks);
    cfg->blocks = NULL;
    cfg->count = 0;
}

static ControlFlowGraph cfg_build(IrFunc *func) {
    ControlFlowGraph cfg = { NULL, 0 };
    size_t capacity = 0;
    BasicBlock *current = NULL;

    for (IrInstr *ins = func->first; ins; ins = ins->next) {
        if (!current || ins->op == IR_LABEL) {
            if (cfg.count == capacity) {
                capacity = capacity ? capacity * 2 : 8;
                cfg.blocks = realloc(cfg.blocks, capacity * sizeof *cfg.blocks);
                if (!cfg.blocks) {
                    fprintf(stderr, "ERRO: sem memoria ao montar o fluxo da RI.\n");
                    exit(1);
                }
            }
            current = &cfg.blocks[cfg.count++];
            memset(current, 0, sizeof *current);
            current->first = ins;
        }
        current->last = ins;
        if (ins->op == IR_GOTO || ins->op == IR_IF_GOTO ||
            ins->op == IR_IF_FALSE || ins->op == IR_RETURN)
            current = NULL;
    }

    if (!cfg.count) return cfg;

    for (size_t i = 0; i < cfg.count; i++) {
        BasicBlock *block = &cfg.blocks[i];
        IrInstr *last = block->last;
        int falls_through = last->op != IR_GOTO && last->op != IR_RETURN;

        if (last->op == IR_GOTO || last->op == IR_IF_GOTO ||
            last->op == IR_IF_FALSE) {
            size_t target = block_index_for_label(&cfg, last->res.name);
            if (target < cfg.count)
                block->successor[block->successor_count++] = (int)target;
        }
        if (falls_through && i + 1 < cfg.count) {
            int next = (int)(i + 1);
            if (block->successor_count == 0 || block->successor[0] != next)
                block->successor[block->successor_count++] = next;
        }
    }

    cfg.blocks[0].reachable = 1;
    for (size_t i = 0; i < cfg.count; i++) {
        if (!cfg.blocks[i].reachable) continue;
        for (int j = 0; j < cfg.blocks[i].successor_count; j++)
            cfg.blocks[cfg.blocks[i].successor[j]].reachable = 1;
    }
    /* A forward pass is insufficient for back edges, so complete reachability
     * with a small fixed-point iteration. */
    int changed;
    do {
        changed = 0;
        for (size_t i = 0; i < cfg.count; i++) {
            if (!cfg.blocks[i].reachable) continue;
            for (int j = 0; j < cfg.blocks[i].successor_count; j++) {
                BasicBlock *next = &cfg.blocks[cfg.blocks[i].successor[j]];
                if (!next->reachable) {
                    next->reachable = 1;
                    changed = 1;
                }
            }
        }
    } while (changed);
    return cfg;
}

static int predecessor_reaches(const ControlFlowGraph *cfg, size_t block,
                               size_t predecessor) {
    const BasicBlock *pred = &cfg->blocks[predecessor];
    for (int i = 0; i < pred->successor_count; i++)
        if ((size_t)pred->successor[i] == block) return 1;
    return 0;
}

static void compute_block_out(const BasicBlock *block,
                              const ConstBinding *in,
                              ConstBinding **out) {
    *out = bindings_clone(in);
    for (IrInstr *ins = block->first; ins; ins = ins->next) {
        transfer_instruction(out, ins, 0);
        if (ins == block->last) break;
    }
}

static void solve_constants(ControlFlowGraph *cfg) {
    if (!cfg->count) return;
    cfg->blocks[0].facts_valid = 1;

    int changed;
    do {
        changed = 0;
        for (size_t i = 0; i < cfg->count; i++) {
            BasicBlock *block = &cfg->blocks[i];
            if (!block->reachable) continue;

            ConstBinding *new_in = NULL;
            int have_predecessor = i == 0;
            for (size_t p = 0; p < cfg->count; p++) {
                BasicBlock *pred = &cfg->blocks[p];
                if (!pred->reachable || !pred->facts_valid ||
                    !predecessor_reaches(cfg, i, p))
                    continue;
                if (i == 0) {
                    bindings_clear(&new_in);
                    have_predecessor = 1;
                    break;
                }
                if (!have_predecessor) {
                    new_in = bindings_clone(pred->out);
                    have_predecessor = 1;
                } else {
                    bindings_intersect(&new_in, pred->out);
                }
            }
            if (!have_predecessor) {
                bindings_clear(&new_in);
                continue;
            }

            ConstBinding *new_out = NULL;
            compute_block_out(block, new_in, &new_out);
            if (!block->facts_valid || !bindings_equal(block->in, new_in) ||
                !bindings_equal(block->out, new_out)) {
                bindings_clear(&block->in);
                bindings_clear(&block->out);
                block->in = new_in;
                block->out = new_out;
                block->facts_valid = 1;
                changed = 1;
            } else {
                bindings_clear(&new_in);
                bindings_clear(&new_out);
            }
        }
    } while (changed);
}

static void rewrite_constants(ControlFlowGraph *cfg) {
    for (size_t i = 0; i < cfg->count; i++) {
        BasicBlock *block = &cfg->blocks[i];
        if (!block->reachable || !block->facts_valid) continue;
        ConstBinding *facts = bindings_clone(block->in);
        for (IrInstr *ins = block->first; ins; ins = ins->next) {
            transfer_instruction(&facts, ins, 1);
            if (ins->op == IR_IF_GOTO || ins->op == IR_IF_FALSE) {
                if (ins->arg1.kind == IR_OP_CONST) {
                    int truth = is_real(ins->arg1.type)
                        ? ins->arg1.value.d != 0.0
                        : ins->arg1.value.i != 0;
                    int take_jump = ins->op == IR_IF_GOTO ? truth : !truth;
                    if (take_jump) {
                        ir_operand_release(&ins->arg1);
                        ins->arg1 = ir_empty();
                        ins->op = IR_GOTO;
                    } else {
                        ir_operand_release(&ins->arg1);
                        ir_operand_release(&ins->res);
                        ins->arg1 = ir_empty();
                        ins->res = ir_empty();
                        ins->op = IR_NOP;
                    }
                }
            }
            if (ins == block->last) break;
        }
        bindings_clear(&facts);
    }
}

static void free_instruction(IrInstr *ins) {
    ir_operand_release(&ins->res);
    ir_operand_release(&ins->arg1);
    ir_operand_release(&ins->arg2);
    free(ins->print_string);
    free(ins);
}

static int remove_unreachable_blocks(IrFunc *func) {
    ControlFlowGraph cfg = cfg_build(func);
    int changed = 0;
    IrInstr **link = &func->first;

    for (size_t block_index = 0; block_index < cfg.count; block_index++) {
        BasicBlock *block = &cfg.blocks[block_index];
        while (*link && *link != block->first) link = &(*link)->next;
        if (!*link) break;

        if (block->reachable) {
            while (*link) {
                IrInstr *ins = *link;
                link = &ins->next;
                if (ins == block->last) break;
            }
            continue;
        }

        while (*link) {
            IrInstr *dead = *link;
            int done = dead == block->last;
            *link = dead->next;
            free_instruction(dead);
            changed = 1;
            if (done) break;
        }
    }
    func->last = NULL;
    for (IrInstr *ins = func->first; ins; ins = ins->next) func->last = ins;
    cfg_free(&cfg);
    return changed;
}

static int is_label_referenced(const IrFunc *func, const char *label) {
    for (const IrInstr *ins = func->first; ins; ins = ins->next) {
        if ((ins->op == IR_GOTO || ins->op == IR_IF_GOTO ||
             ins->op == IR_IF_FALSE) &&
            ins->res.name && strcmp(ins->res.name, label) == 0)
            return 1;
    }
    return 0;
}

static int clean_jumps_and_labels(IrFunc *func) {
    int changed = 0;
    IrInstr **link = &func->first;
    while (*link) {
        IrInstr *ins = *link;
        if (ins->op == IR_NOP) {
            *link = ins->next;
            free_instruction(ins);
            changed = 1;
            continue;
        }
        if (ins->op == IR_GOTO && ins->next && ins->next->op == IR_LABEL &&
            ins->res.name && ins->next->res.name &&
            strcmp(ins->res.name, ins->next->res.name) == 0) {
            *link = ins->next;
            free_instruction(ins);
            changed = 1;
            continue;
        }
        if (ins->op == IR_LABEL && ins->res.name &&
            !is_label_referenced(func, ins->res.name)) {
            *link = ins->next;
            free_instruction(ins);
            changed = 1;
            continue;
        }
        link = &ins->next;
    }
    func->last = NULL;
    for (IrInstr *ins = func->first; ins; ins = ins->next) func->last = ins;
    return changed;
}

static int operand_uses_temp(const IrOperand *op, int id) {
    return op->kind == IR_OP_TEMP && op->id == id;
}

static int temp_use_count(const IrFunc *func, int id) {
    int uses = 0;
    for (const IrInstr *ins = func->first; ins; ins = ins->next) {
        if (operand_uses_temp(&ins->arg1, id)) uses++;
        if (operand_uses_temp(&ins->arg2, id)) uses++;
    }
    return uses;
}

static int remove_dead_temporaries(IrFunc *func) {
    int changed = 0;
    int repeat;
    do {
        repeat = 0;
        for (IrInstr **link = &func->first; *link;) {
            IrInstr *ins = *link;
            int pure = ins->op == IR_ASSIGN || ins->op == IR_UNARY ||
                       ins->op == IR_BINOP || ins->op == IR_CAST;
            if (pure && ins->res.kind == IR_OP_TEMP &&
                temp_use_count(func, ins->res.id) == 0) {
                *link = ins->next;
                free_instruction(ins);
                changed = repeat = 1;
            } else {
                if (ins->op == IR_CALL && ins->res.kind == IR_OP_TEMP &&
                    temp_use_count(func, ins->res.id) == 0) {
                    ir_operand_release(&ins->res);
                    ins->res = ir_empty();
                    changed = repeat = 1;
                }
                link = &ins->next;
            }
        }
    } while (repeat);

    func->last = NULL;
    for (IrInstr *ins = func->first; ins; ins = ins->next) func->last = ins;
    return changed;
}

static void mark_temp(const IrOperand *op, unsigned char *used, int count) {
    if (op->kind == IR_OP_TEMP && op->id >= 0 && op->id < count)
        used[op->id] = 1;
}

static int compact_temporaries(IrFunc *func) {
    int count = func->temp_count;
    if (count <= 0) return 0;
    unsigned char *used = opt_calloc((size_t)count, sizeof *used);
    int *map = opt_calloc((size_t)count, sizeof *map);
    for (IrInstr *ins = func->first; ins; ins = ins->next) {
        mark_temp(&ins->res, used, count);
        mark_temp(&ins->arg1, used, count);
        mark_temp(&ins->arg2, used, count);
    }

    int next_id = 0;
    for (int i = 0; i < count; i++) {
        map[i] = used[i] ? next_id++ : -1;
    }
    for (IrInstr *ins = func->first; ins; ins = ins->next) {
        IrOperand *ops[] = { &ins->res, &ins->arg1, &ins->arg2 };
        for (size_t i = 0; i < sizeof ops / sizeof ops[0]; i++)
            if (ops[i]->kind == IR_OP_TEMP && ops[i]->id >= 0 &&
                ops[i]->id < count)
                ops[i]->id = map[ops[i]->id];
    }
    func->temp_count = next_id;
    free(used);
    free(map);
    return next_id != count;
}

static int expression_matches(const CseEntry *entry, const IrInstr *ins) {
    return entry->op == ins->op && entry->type == ins->type &&
           entry->unop == ins->unop && entry->binop == ins->binop &&
           entry->cast_type == ins->cast_type &&
           operands_equal(&entry->arg1, &ins->arg1) &&
           operands_equal(&entry->arg2, &ins->arg2);
}

static int expression_uses(const CseEntry *entry, const IrOperand *key) {
    return operands_equal(&entry->arg1, key) ||
           operands_equal(&entry->arg2, key);
}

static void cse_remove_uses(CseEntry **entries, const IrOperand *key) {
    CseEntry **link = entries;
    while (*link) {
        CseEntry *entry = *link;
        if (expression_uses(entry, key) ||
            operands_equal(&entry->result, key)) {
            *link = entry->next;
            ir_operand_release(&entry->arg1);
            ir_operand_release(&entry->arg2);
            ir_operand_release(&entry->result);
            free(entry);
        } else {
            link = &entry->next;
        }
    }
}

static void cse_clear(CseEntry **entries) {
    while (*entries) {
        CseEntry *next = (*entries)->next;
        ir_operand_release(&(*entries)->arg1);
        ir_operand_release(&(*entries)->arg2);
        ir_operand_release(&(*entries)->result);
        free(*entries);
        *entries = next;
    }
}

static int expression_opcode(IrOpcode op) {
    return op == IR_UNARY || op == IR_BINOP || op == IR_CAST;
}

static int run_local_cse(IrFunc *func) {
    int changed = 0;
    CseEntry *entries = NULL;

    for (IrInstr *ins = func->first; ins; ins = ins->next) {
        if (ins->op == IR_LABEL || ins->op == IR_GOTO ||
            ins->op == IR_IF_GOTO || ins->op == IR_IF_FALSE) {
            cse_clear(&entries);
            continue;
        }
        if (ins->op == IR_CALL) {
            cse_clear(&entries);
            continue;
        }

        if (ins->op == IR_ASSIGN) {
            if (ins->res.kind == IR_OP_VAR || ins->res.kind == IR_OP_TEMP)
                cse_remove_uses(&entries, &ins->res);
            continue;
        }
        if (!expression_opcode(ins->op) || ins->res.kind != IR_OP_TEMP)
            continue;

        cse_remove_uses(&entries, &ins->res);
        CseEntry *match = NULL;
        for (CseEntry *entry = entries; entry; entry = entry->next)
            if (expression_matches(entry, ins)) {
                match = entry;
                break;
            }
        if (match) {
            ir_operand_release(&ins->arg1);
            ir_operand_release(&ins->arg2);
            ins->op = IR_ASSIGN;
            ins->arg1 = operand_copy(&match->result);
            ins->arg2 = ir_empty();
            ins->unop = 0;
            ins->binop = 0;
            ins->cast_type = TYPE_UNKNOWN;
            cse_remove_uses(&entries, &ins->res);
            changed = 1;
            continue;
        }

        CseEntry *entry = opt_calloc(1, sizeof *entry);
        entry->op = ins->op;
        entry->type = ins->type;
        entry->unop = ins->unop;
        entry->binop = ins->binop;
        entry->cast_type = ins->cast_type;
        entry->arg1 = operand_copy(&ins->arg1);
        entry->arg2 = operand_copy(&ins->arg2);
        entry->result = operand_copy(&ins->res);
        entry->next = entries;
        entries = entry;
    }

    cse_clear(&entries);
    return changed;
}

static int optimize_function(IrFunc *func) {
    int changed = 0;
    ControlFlowGraph cfg = cfg_build(func);
    solve_constants(&cfg);
    rewrite_constants(&cfg);
    cfg_free(&cfg);

    changed |= remove_unreachable_blocks(func);
    changed |= clean_jumps_and_labels(func);
    changed |= run_local_cse(func);
    changed |= remove_dead_temporaries(func);
    changed |= compact_temporaries(func);
    return changed;
}

void ir_optimize(IrProgram *program) {
    if (!program) return;
    for (IrFunc *func = program->funcs; func; func = func->next) {
        int changed;
        do {
            changed = optimize_function(func);
        } while (changed);
    }
}
