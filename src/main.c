/*
 * main.c
 *
 * Ponto de entrada do prototipo Flex + Bison. Responsavel por abrir o
 * arquivo de entrada, disparar a analise (yyparse), reportar o resultado
 * e imprimir a AST montada pelo parser. A gramatica em si vive em
 * parser/parser.y e o analisador lexico em lexer/lexer.l.
 *
 * Uso:  protoparser [--ast] [arquivo]
 *       --ast  imprime SO a AST, com a linha de cada no ("@N") e sem as
 *              mensagens de inicio/fim; formato usado por tests/check_ast.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

/* Gerados por Bison a partir de parser/parser.y (ver parser.tab.h) */
int yyparse(void);
extern FILE *yyin;

/* Gerada pelo Flex: libera os buffers internos do lexer */
int yylex_destroy(void);

/* Definida em lexer/lexer.l — ver comentario la para o porque disso importa */
extern int had_lexical_error;

int main(int argc, char **argv) {
    int only_ast = 0;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ast") == 0) only_ast = ast_print_lines = 1;
        else path = argv[i];
    }

    if (path) {
        yyin = fopen(path, "r");
        if (!yyin) {
            fprintf(stderr, "Nao foi possivel abrir o arquivo: %s\n", path);
            return 1;
        }
    }

    if (!only_ast) printf("=== Prototipo Flex + Bison: iniciando analise ===\n");
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
        if (!only_ast) printf(">> Programa reconhecido com sucesso.\n\n--- AST ---\n");
        print_ast(ast_root, 0);
        if (!only_ast) printf("\n=== Analise concluida sem erros ===\n");
    } else if (!only_ast) {
        printf("=== Analise concluida com erros ===\n");
    }

    free_ast(ast_root);
    ast_root = NULL;

    if (path) {
        fclose(yyin);
    }
    yylex_destroy();

    return success ? 0 : 1;
}
