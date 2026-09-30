/*
 * main.c
 *
 * Ponto de entrada do prototipo Flex + Bison. Responsavel por abrir o
 * arquivo de entrada, disparar a analise (yyparse) e reportar o
 * resultado. A gramatica em si vive em parser/parser.y e o analisador
 * lexico em lexer/lexer.l.
 */

#include <stdio.h>
#include <stdlib.h>

/* Gerados por Bison a partir de parser/parser.y (ver parser.tab.h) */
int yyparse(void);
extern FILE *yyin;

/* Definida em lexer/lexer.l — ver comentario la para o porque disso importa */
extern int had_lexical_error;

int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            fprintf(stderr, "Nao foi possivel abrir o arquivo: %s\n", argv[1]);
            return 1;
        }
    }

    printf("=== Prototipo Flex + Bison: iniciando analise ===\n");
    int parse_result = yyparse();

    /*
     * A analise so e considerada bem-sucedida se o parser nao reportou
     * erro sintatico E o lexer nao reportou nenhum erro lexico ao longo
     * do caminho. Isso evita o cenario em que um token invalido e
     * descartado silenciosamente pelo lexer e o restante da entrada
     * ainda forma uma sentenca sintaticamente valida — nesse caso
     * yyparse() sozinho retornaria 0, mascarando o erro lexico.
     */
    int success = (parse_result == 0) && !had_lexical_error;

    if (success) {
        printf("=== Analise concluida sem erros ===\n");
    } else {
        printf("=== Analise concluida com erros ===\n");
    }

    if (argc > 1) {
        fclose(yyin);
    }

    return success ? 0 : 1;
}
