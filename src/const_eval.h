#ifndef CONST_EVAL_H
#define CONST_EVAL_H

/*
 * const_eval.h
 *
 * Avaliação de expressões constantes em tempo de compilação, no sentido
 * do Java (JLS 15.29) restrito ao subconjunto: literais e operadores
 * aplicados a literais (não há 'final', então nenhuma variável é
 * constante).
 *
 * Usado pela análise semântica (condições constantes em laços/if para
 * alcançabilidade e inicialização de variáveis; 'char c = 65') e pela
 * geração da IR (valor inicial de globais).
 */

#include "ast.h"

typedef struct {
    Type   type;  /* TYPE_INT, TYPE_CHAR, TYPE_BOOLEAN, TYPE_FLOAT, TYPE_DOUBLE */
    long   i;     /* int (32 bits, com wrap-around), char (0..65535), boolean (0/1) */
    double d;     /* float/double */
} ConstVal;

/* Avalia 'n'. Retorna 1 e preenche *out se 'n' é uma expressão constante;
 * 0 caso contrário (inclui divisão inteira por zero, que não é constante). */
int const_eval(Node *n, ConstVal *out);

/* Converte uma constante para o tipo 't' com as regras de cast do Java. */
ConstVal const_convert(ConstVal c, Type t);

/* 1 se 'n' é constante boolean e, nesse caso, *value recebe 0/1. */
int const_bool(Node *n, int *value);

/* Valor de um literal char a partir do texto cru (ex.: "a", "\n"). */
long char_literal_value(const char *text);

#endif /* CONST_EVAL_H */
