/*
 * main.c
 *
 * Ponto de entrada do compilador. Responsável por abrir o arquivo de
 * entrada, inicializar a tabela de símbolos, disparar a análise (yyparse),
 * reportar o resultado e imprimir a AST montada pelo parser. A gramática
 * vive em parser/parser.y, o analisador léxico em lexer/lexer.l, a tabela
 * de símbolos em src/symbol_table.c e a AST em parser/ast.c.
 *
 * Uso:
 *   ./protoparser arquivo.java
 *   ./protoparser --tabela arquivo.java   (imprime a tabela de símbolos)
 *   protoparser [--ast] [arquivo]
 *       --ast  imprime SÓ a AST, com a linha de cada nó ("@N") e sem as
 *              mensagens de início/fim; formato usado por tests/check_ast.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"
#include "ast.h"

/* Gerados por Bison a partir de parser/parser.y (ver parser.tab.h) */
int yyparse(void);
extern FILE *yyin;

/* Gerada pelo Flex: libera os buffers internos do lexer */
int yylex_destroy(void);

/* Definidas em lexer/lexer.l e parser/parser.y */
extern int had_lexical_error;
extern int had_semantic_error;
extern int show_symbol_table;

static void usage(const char *program) {
    fprintf(stderr, "uso: %s [--tabela|--ast] [--ajuda] [arquivo.java]\n", program);
}

int main(int argc, char **argv) {
    int only_ast = 0;
    const char *input_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tabela") == 0 || strcmp(argv[i], "-t") == 0) {
            show_symbol_table = 1;
        } else if (strcmp(argv[i], "--ast") == 0) {
            only_ast = ast_print_lines = 1;
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

    if (!only_ast) printf("=== Prototipo Flex + Bison: iniciando analise ===\n");
    int parse_result = yyparse();

    /*
     * A análise só é considerada bem-sucedida se o parser não reportou
     * erro sintático, o lexer não reportou erro léxico e a análise
     * semântica (tabela de símbolos) não reportou erro.
     */
    int success = (parse_result == 0) && !had_lexical_error && !had_semantic_error;

    if (success) {
        if (!only_ast) printf(">> Programa reconhecido com sucesso.\n\n--- AST ---\n");
        print_ast(ast_root, 0);
        if (!only_ast) printf("\n=== Analise concluida sem erros ===\n");
    } else if (!only_ast) {
        printf("=== Analise concluida com erros ===\n");
    }

    free_ast(ast_root);
    ast_root = NULL;

    if (input_path) {
        fclose(yyin);
    }
    yylex_destroy();

    st_free();
    return success ? 0 : 1;
}
