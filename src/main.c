/*
 * main.c
 *
 * Ponto de entrada do compilador. Responsavel por abrir o arquivo de
 * entrada, inicializar a tabela de simbolos, disparar a analise (yyparse)
 * e reportar o resultado. A gramatica vive em parser/parser.y, o analisador
 * lexico em lexer/lexer.l e a tabela de simbolos em src/symbol_table.c.
 *
 * Uso:
 *   ./protoparser arquivo.java
 *   ./protoparser --tabela arquivo.java   (imprime a tabela de simbolos)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"

/* Gerados por Bison a partir de parser/parser.y (ver parser.tab.h) */
int yyparse(void);
extern FILE *yyin;

/* Definidas em lexer/lexer.l e parser/parser.y */
extern int had_lexical_error;
extern int had_semantic_error;
extern int show_symbol_table;

static void usage(const char *program) {
    fprintf(stderr, "uso: %s [--tabela] arquivo.java\n", program);
}

int main(int argc, char **argv) {
    const char *input_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tabela") == 0 || strcmp(argv[i], "-t") == 0) {
            show_symbol_table = 1;
        } else if (strcmp(argv[i], "--ajuda") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (input_path == NULL) {
            input_path = argv[i];
        } else {
            fprintf(stderr, "argumento inesperado: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (input_path) {
        yyin = fopen(input_path, "r");
        if (!yyin) {
            fprintf(stderr, "Nao foi possivel abrir o arquivo: %s\n", input_path);
            return 1;
        }
    }

    st_init();

    printf("=== Prototipo Flex + Bison: iniciando analise ===\n");
    int parse_result = yyparse();

    /*
     * A analise so e considerada bem-sucedida se o parser nao reportou
     * erro sintatico, o lexer nao reportou erro lexico e a analise
     * semantica (tabela de simbolos) nao reportou erro.
     */
    int success = (parse_result == 0) && !had_lexical_error && !had_semantic_error;

    if (success) {
        printf("=== Analise concluida sem erros ===\n");
    } else {
        printf("=== Analise concluida com erros ===\n");
    }

    if (input_path) {
        fclose(yyin);
    }

    st_free();
    return success ? 0 : 1;
}
