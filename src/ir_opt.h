#ifndef IR_OPT_H
#define IR_OPT_H

#include "ir.h"

/* Otimiza a RI em lugar, sem alterar a estrutura das instrucoes ou funcoes. */
void ir_optimize(IrProgram *program);

#endif
