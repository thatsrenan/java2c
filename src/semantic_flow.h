#ifndef SEMANTIC_FLOW_H
#define SEMANTIC_FLOW_H

/*
 * semantic_flow.h
 *
 * Análise semântica de fluxo, feita sobre a AST completa depois do parser
 * (as verificações de tipo e escopo acontecem durante o parsing). Segue
 * as regras do Java (JLS cap. 14.22 e 16) restritas ao subconjunto:
 *
 *  - código inalcançável é erro (ex.: comando depois de return/break,
 *    corpo de while(false));
 *  - método com retorno não-void precisa retornar em todos os caminhos
 *    (especificação, seção 8);
 *  - variável local precisa estar definitivamente atribuída antes de ser
 *    lida (definite assignment, especificação, seção 3). Condições
 *    constantes contam como no javac: 'if (true) { x = 5; }' atribui x.
 *
 * Erros saem em stderr no formato "ERRO SEMANTICO na linha N: ...".
 */

#include "ast.h"

/* Analisa o programa; retorna o número de erros encontrados. */
int semantic_flow_check(Node *root);

#endif /* SEMANTIC_FLOW_H */
