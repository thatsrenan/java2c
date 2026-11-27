# API da IR e geração de C

A representação intermediária é definida em `src/ir.h`, gerada em
`src/ir_gen.h` / `src/ir_gen.c`, otimizada em `src/ir_opt.h` / `src/ir_opt.c`
e traduzida para C em `src/codegen.h` / `src/codegen.c`.

## Estruturas

```c
typedef Type IrType;  /* reusa TYPE_INT, TYPE_DOUBLE, ... da AST */

typedef enum {
    IR_OP_EMPTY, IR_OP_TEMP, IR_OP_VAR,
    IR_OP_CONST, IR_OP_LABEL, IR_OP_FUNC
} IrOperandKind;
```

| Estrutura | Campos principais |
| --- | --- |
| `IrOperand` | `kind`, `type`, `id` (temporários), `name` (variáveis, rótulos, funções), `value.i` / `value.d` (constantes) |
| `IrInstr` | `op`, `type`, `res`, `arg1`, `arg2`, `unop`, `binop`, `cast_type`, `print_string`, `next` |
| `IrParam` | `name`, `type`, `next` |
| `IrLocal` | `name`, `type`, `next` |
| `IrGlobal` | `name`, `type`, `has_init`, `init`, `next` |
| `IrFunc` | `name`, `ret_type`, `params`, `locals`, `first`, `last`, `temp_count`, `next` |
| `IrProgram` | `globals`, `funcs`, `label_count` |

`IrOpcode` cobre movimentação (`IR_ASSIGN`, `IR_UNARY`, `IR_BINOP`,
`IR_CAST`), fluxo (`IR_LABEL`, `IR_GOTO`, `IR_IF_GOTO`, `IR_IF_FALSE`),
funções (`IR_PARAM`, `IR_CALL`, `IR_RETURN`) e saída (`IR_PRINT`). Operadores
de `IR_UNARY` e `IR_BINOP` reaproveitam `OpKind` da AST.

## Construção da IR

| Função | Retorno / efeito |
| --- | --- |
| `IrProgram *ir_program_new(void)` | programa vazio |
| `void ir_program_free(IrProgram *p)` | libera o programa e seus nós |
| `IrFunc *ir_program_add_func(IrProgram *p, const char *name, IrType ret_type)` | acrescenta função e a retorna |
| `void ir_func_add_param(IrFunc *f, const char *name, IrType type)` | anexa parâmetro |
| `void ir_func_add_local(IrFunc *f, const char *name, IrType type)` | anexa local |
| `void ir_program_add_global(IrProgram *p, const char *name, IrType type, int has_init, IrOperand init)` | acrescenta/atualiza global; `init` deve ser constante ou `ir_empty()` |
| `char *ir_new_label(IrProgram *p, const char *prefix)` | nome único de rótulo; chamador libera a string |
| `int ir_new_temp(IrFunc *f)` | próximo id de temporário da função (`t0`, `t1`, ...) |
| `IrInstr *ir_emit(IrFunc *f, IrOpcode op)` | anexa instrução no fim da função |
| `void ir_operand_release(IrOperand *op)` | libera a string pertencente ao operando |
| `void ir_print_program(FILE *out, const IrProgram *p)` | imprime a TAC em formato textual |
| `const char *ir_type_name(IrType t)` | nome legível do tipo |

### Operandos

`ir_empty()`, `ir_temp(int id, IrType type)`,
`ir_var(const char *name, IrType type)`,
`ir_const_int(long v, IrType type)`,
`ir_const_real(double v, IrType type)`,
`ir_label(const char *name)` e
`ir_func_ref(const char *name)` constroem operandos. Strings são copiadas
quando aplicável.

Exemplo de construção direta:

```c
IrProgram *p = ir_program_new();
IrFunc *f = ir_program_add_func(p, "main", TYPE_VOID);
IrInstr *ins = ir_emit(f, IR_PRINT);
ins->arg1 = ir_const_int(42, TYPE_INT);
ir_print_program(stdout, p);
ir_program_free(p);
```

## Geração, otimização e saída

| Função | Contrato |
| --- | --- |
| `IrProgram *ir_generate(Node *root)` | converte a AST validada em IR; aplica as otimizações antes de retornar; o chamador deve chamar `ir_program_free`; a tabela de símbolos precisa estar ativa (`st_init`/`st_free`) |
| `void ir_optimize(IrProgram *program)` | otimiza em lugar, sem alterar a estrutura de instruções ou funções |
| `int codegen_emit(FILE *out, const IrProgram *program)` | emite C com temporários e desvios explícitos; retorna `0` em sucesso e `-1` se a IR estiver malformada |
| `int codegen_emit_clean(FILE *out, const IrProgram *program)` | mesma IR, com expressões em linha e menos ruído; mesmo contrato de retorno |

`ir_generate` pressiona as otimizações internamente, portanto a TAC mostrada
por `--ir` já é a versão otimizada. A IR é a única fonte do gerador de C:
tipos, nomes únicos, curto-circuito, rótulos de `break`/`continue` e
inicialização de globais já estão decididos nela.

O arquivo `out` passado ao gerador é apenas escrito; sua propriedade continua
com o chamador. `src/main.c` usa `stdout` quando `-o` não é informado.

## Análise de fluxo e constantes

Estas rotinas pertencem ao front-end, mas são usadas na mesma cadeia de
decisões da IR:

| Função | Contrato |
| --- | --- |
| `int semantic_flow_check(Node *root)` | verifica alcance, retorno em todos os caminhos e atribuição definitiva; retorna o número de erros |
| `int const_eval(Node *n, ConstVal *out)` | `1` e preenche `out` se a expressão for constante; `0` caso contrário (inclui divisão inteira por zero) |
| `int const_eval_unary(OpKind op, ConstVal operand, ConstVal *out)` | avaliação de operador unário sobre constantes |
| `int const_eval_binary(OpKind op, ConstVal left, ConstVal right, ConstVal *out)` | avaliação de operador binário sobre constantes |
| `ConstVal const_convert(ConstVal c, Type t)` | converte uma constante segundo as regras de cast do Java |
| `int const_bool(Node *n, int *value)` | `1` se a expressão for booleana constante, com `0`/`1` em `value` |
| `long char_literal_value(const char *text)` | valor de um literal `char` a partir do texto cru |

`ConstVal` contém `type`, `i` (int/char/boolean) e `d` (float/double).
Condições constantes contam para alcançabilidade e inicialização, e
inicializadores constantes de campos `static` são usados como valor inicial
das globais na IR.

## Leitura relacionada

- [IR e geração de código](../architecture/ir-and-codegen.md) — formato da
  TAC, nomes únicos e modos de saída.
- [Linha de comando](cli.md) — como invocar cada etapa.
