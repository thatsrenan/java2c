#ifndef IR_H
#define IR_H

/*
 * ir.h
 *
 * Representação intermediária (IR) do compilador Java -> C em forma de
 * TAC (Three-Address Code / código de três endereços).
 *
 * A ideia é servir como ponte entre a AST (já validada pelo front-end) e
 * a futura geração de código C:
 *   - cada instrução faz, no máximo, uma operação elementar;
 *   - expressões complexas são quebradas com temporários (t0, t1, ...);
 *   - o controle de fluxo estruturado (if/while/for/do-while, &&, ||,
 *     break/continue) vira rótulos e saltos explícitos.
 *
 * Esta IR é deliberadamente simples e didática, pensada para o
 * subconjunto procedural suportado pelo projeto.
 */

#include <stdio.h>
#include "ast.h" /* Type */

/* ===================== Tipos e operandos ===================== */

/* Tipo "de máquina" usado na IR. Reaproveitamos o enum Type da AST para
 * não duplicar conceitos (TYPE_INT, TYPE_FLOAT, TYPE_DOUBLE,
 * TYPE_BOOLEAN, TYPE_CHAR, TYPE_VOID, TYPE_STRING para literais de
 * println, etc.). TYPE_UNKNOWN pode aparecer como fallback, mas o
 * front-end já deve ter validado os programas aceitos. */
typedef Type IrType;

/* Categoria de um operando (endereço) de instrução TAC. */
typedef enum {
    IR_OP_EMPTY,   /* ausência de operando (campo não usado) */
    IR_OP_TEMP,    /* temporário gerado pelo compilador: tN */
    IR_OP_VAR,     /* variável/param global ou local, ou parâmetro: nome */
    IR_OP_CONST,   /* constante imediata (int/float/double/char/bool) */
    IR_OP_LABEL,   /* rótulo de código: Lnome/ Ln */
    IR_OP_FUNC     /* nome de função/método (alvo de call) */
} IrOperandKind;

/* Operando de uma instrução.
 *
 * - TEMP:  id = número do temporário (impresso como t<id>).
 * - VAR:   name = identificador no programa.
 * - CONST: type indica o tipo da constante e o campo de 'value' guarda
 *          o dado (i para int/char/boolean; d para float/double).
 * - LABEL: name = nome do rótulo.
 * - FUNC:  name = nome da função.
 * - EMPTY: não utilizado.
 *
 * 'type' carrega o tipo do operando quando relevante (temporários,
 * variáveis, constantes), o que ajuda a futura geração de C.
 */
typedef struct {
    IrOperandKind kind;
    IrType        type;
    int           id;      /* usado por TEMP */
    char         *name;    /* usado por VAR/LABEL/FUNC (string própria) */
    union {
        long   i;          /* int, char (código), boolean (0/1) */
        double d;          /* float/double */
    } value;               /* usado por CONST */
} IrOperand;

/* ===================== Instruções ===================== */

/* Código de operação das instruções TAC. */
typedef enum {
    IR_NOP,       /* sem efeito (pode ser útil como marcador) */

    /* Movimentação / valores */
    IR_ASSIGN,    /* res = arg1                     (cópia simples) */
    IR_UNARY,     /* res = op arg1                  (op em 'unop')   */
    IR_BINOP,     /* res = arg1 op arg2             (op em 'binop')  */
    IR_CAST,      /* res = (cast_type) arg1                           */

    /* Controle de fluxo */
    IR_LABEL,     /* res: label                                      */
    IR_GOTO,      /* goto res(label)                                 */
    IR_IF_GOTO,   /* if (arg1) goto res(label)      (condição true)  */
    IR_IF_FALSE,  /* if (!arg1) goto res(label)     (condição false) */

    /* Funções */
    IR_PARAM,     /* param arg1                     (empilha argumento) */
    IR_CALL,      /* res = call arg1(func), arg2(const int = nargs)  */
                  /* se res for EMPTY, chamada sem valor aproveitado  */
    IR_RETURN,    /* return arg1   (ou return vazio se arg1 EMPTY)    */

    /* Saída simplificada (System.out.println) */
    IR_PRINT      /* print arg1; se 'print_string' != NULL, imprime   */
                  /* a string literal em vez de arg1                  */
} IrOpcode;

/* Operadores para IR_UNARY / IR_BINOP (reaproveitam OpKind da AST). */
typedef OpKind IrUnOp;
typedef OpKind IrBinOp;

/* Uma instrução/quádrupla TAC. */
typedef struct IrInstr {
    IrOpcode   op;
    IrType     type;         /* tipo do resultado/valor produzido, quando houver */
    IrOperand  res;
    IrOperand  arg1;
    IrOperand  arg2;
    IrUnOp     unop;         /* válido se op == IR_UNARY */
    IrBinOp    binop;        /* válido se op == IR_BINOP */
    IrType     cast_type;    /* válido se op == IR_CAST: tipo de destino */
    char      *print_string; /* válido se op == IR_PRINT: literal (sem aspas) */

    struct IrInstr *next;    /* próxima instrução na lista da função */
} IrInstr;

/* ===================== Programa / funções / globais ===================== */

/* Parâmetro formal de uma função na IR (nome + tipo). */
typedef struct IrParam {
    char           *name;
    IrType          type;
    struct IrParam *next;
} IrParam;

/* Declaração de variável global/campo static. */
typedef struct IrGlobal {
    char            *name;
    IrType           type;
    int              has_init;  /* 1 se possui inicializador constante */
    IrOperand        init;      /* valor inicial (IR_OP_CONST) quando has_init */
    struct IrGlobal *next;
} IrGlobal;

/* IR de uma função/método. */
typedef struct IrFunc {
    char          *name;
    IrType         ret_type;
    IrParam       *params;      /* lista de parâmetros, em ordem */
    IrInstr       *first;       /* primeira instrução */
    IrInstr       *last;        /* última instrução (para anexar em O(1)) */
    int            temp_count;  /* quantidade de temporários usados */
    struct IrFunc *next;
} IrFunc;

/* Programa inteiro em TAC. */
typedef struct {
    IrGlobal *globals;
    IrFunc   *funcs;
    int       label_count;  /* contador global para rótulos únicos */
    int       temp_count;   /* contador global para temporários únicos */
} IrProgram;

/* ===================== API de construção ===================== */

/* Cria/libera um programa IR vazio. */
IrProgram *ir_program_new(void);
void       ir_program_free(IrProgram *p);

/* Adiciona uma função ao programa e a retorna. */
IrFunc *ir_program_add_func(IrProgram *p, const char *name, IrType ret_type);

/* Adiciona um parâmetro ao final da lista de parâmetros da função. */
void ir_func_add_param(IrFunc *f, const char *name, IrType type);

/* Adiciona/atualiza uma variável global. 'init' deve ser um operando
 * constante (IR_OP_CONST) ou EMPTY se não houver inicializador. */
void ir_program_add_global(IrProgram *p, const char *name, IrType type,
                           int has_init, IrOperand init);

/* Cria operandos (as strings são copiadas quando aplicável). */
IrOperand ir_empty(void);
IrOperand ir_temp(int id, IrType type);
IrOperand ir_var(const char *name, IrType type);
IrOperand ir_const_int(long v, IrType type);      /* int/char/bool */
IrOperand ir_const_real(double v, IrType type);   /* float/double */
IrOperand ir_label(const char *name);
IrOperand ir_func_ref(const char *name);

/* Gera nomes/ids únicos no contexto do programa. O chamador é dono da
 * string retornada por ir_new_label (deve liberá-la quando não precisar
 * mais, por exemplo após construir o operando/rótulo). */
char *ir_new_label(IrProgram *p, const char *prefix);
int   ir_new_temp(IrProgram *p);

/* Emite (anexa) uma instrução no fim da função e a retorna. */
IrInstr *ir_emit(IrFunc *f, IrOpcode op);

/* Libera memória própria de um operando (nome duplicado). Útil ao
 * descartar operandos temporários que não foram anexados. */
void ir_operand_release(IrOperand *op);

/* ===================== Impressão ===================== */

/* Imprime o programa TAC em formato textual legível. */
void ir_print_program(FILE *out, const IrProgram *p);

/* Nome legível de um tipo da IR (delegando para type_name da AST). */
const char *ir_type_name(IrType t);

#endif /* IR_H */
