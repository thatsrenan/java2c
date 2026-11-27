#ifndef CODEGEN_H
#define CODEGEN_H

/*
 * codegen.h
 *
 * Geração de código C (C99/C11) a partir da IR (TAC) já validada e
 * otimizada. A representação intermediária é a única fonte: todas as
 * decisões semânticas (tipos, nomes únicos, curto-circuito, rótulos de
 * break/continue e inicialização de globais) já foram tomadas antes.
 */

#include <stdio.h>

#include "ir.h"

/* Emite o programa C em 'out'. Retorna 0 em sucesso e -1 se a IR estiver
 * malformada (não emitindo código C completo nesse caso). O arquivo 'out'
 * é apenas escrito; sua propriedade continua com o chamador. */
int codegen_emit(FILE *out, const IrProgram *program);

/* Variante legível: continua gerando C a partir da mesma IR, mas remove
 * temporários de uso único (quando a substituição é segura), omite casts
 * (void) de variáveis realmente usadas e evita ruído desnecessário. */
int codegen_emit_clean(FILE *out, const IrProgram *program);

#endif /* CODEGEN_H */
