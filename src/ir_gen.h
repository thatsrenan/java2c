#ifndef IR_GEN_H
#define IR_GEN_H

/*
 * ir_gen.h
 *
 * Geração de TAC (IR) a partir da AST do compilador.
 *
 * A entrada esperada é a AST construída e validada pelo parser/análise
 * semântica (ast_root). A saída é um IrProgram contendo globais e a IR
 * de cada método/função.
 */

#include "ast.h"
#include "ir.h"

/* Gera o programa IR a partir da raiz da AST. Retorna um IrProgram novo
 * (o chamador deve liberá-lo com ir_program_free). Se 'root' for NULL,
 * retorna um programa vazio.
 *
 * Pré-requisito: a tabela de símbolos precisa ter sido inicializada e
 * ainda estar ativa durante a chamada (st_init/st_free). O gerador
 * consulta o escopo global para descobrir, por exemplo, o tipo de
 * retorno de funções (e assim não criar temporário para chamadas void). */
IrProgram *ir_generate(Node *root);

#endif /* IR_GEN_H */
