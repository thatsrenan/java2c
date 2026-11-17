/*
 * main.c
 *
 * Ponto de entrada do compilador. Responsável por abrir o arquivo de
 * entrada, inicializar a tabela de símbolos, disparar a análise (yyparse),
 * reportar o resultado e (conforme a opção) imprimir a AST ou a IR (TAC)
 * gerada a partir da AST. A gramática vive em parser/parser.y, o
 * analisador léxico em lexer/lexer.l, a tabela de símbolos em
 * src/symbol_table.c, a análise de fluxo em src/semantic_flow.c, a AST em
 * parser/ast.c e a IR em src/ir.c/src/ir_gen.c.
 *
 * Uso:
 *   ./protoparser arquivo.java
 *   ./protoparser --tabela arquivo.java   (imprime a tabela de símbolos)
 *   ./protoparser --ast [arquivo]         (imprime só a AST, com @linha)
 *   ./protoparser --ir  [arquivo]         (imprime só a IR/TAC)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"
#include "ast.h"
#include "ir.h"
#include "ir_gen.h"
#include "semantic_flow.h"

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
    fprintf(stderr, "uso: %s [--tabela|--ast|--ir] [--ajuda] [arquivo.java]\n", program);
}

int main(int argc, char **argv) {
    int only_ast = 0;
    int only_ir = 0;
    const char *input_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tabela") == 0 || strcmp(argv[i], "-t") == 0) {
            show_symbol_table = 1;
        } else if (strcmp(argv[i], "--ast") == 0) {
            only_ast = ast_print_lines = 1;
        } else if (strcmp(argv[i], "--ir") == 0) {
            only_ir = 1;
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

    int quiet = only_ast || only_ir;
    if (!quiet) printf("=== Prototipo Flex + Bison: iniciando analise ===\n");
    int parse_result = yyparse();

    /*
     * Com a AST completa, a análise de fluxo verifica o que depende do
     * programa inteiro: código inalcançável, retorno em todos os caminhos
     * e variáveis locais inicializadas antes do uso.
     */
    if (parse_result == 0 && !had_lexical_error && ast_root) {
        if (semantic_flow_check(ast_root) > 0) had_semantic_error = 1;
    }

    /*
     * A análise só é considerada bem-sucedida se o parser não reportou
     * erro sintático, o lexer não reportou erro léxico e a análise
     * semântica (tabela de símbolos) não reportou erro.
     */
    int success = (parse_result == 0) && !had_lexical_error && !had_semantic_error;

    /* Estrutura para a IR: gerada apenas quando a análise teve sucesso. */
    IrProgram *ir_prog = NULL;

    if (success) {
        if (only_ir) {
            /* Modo IR: imprime somente a representação intermediária. */
            ir_prog = ir_generate(ast_root);
            ir_print_program(stdout, ir_prog);
        } else if (only_ast) {
            /* Modo AST: imprime somente a AST (com @linha). */
            print_ast(ast_root, 0);
        } else {
            printf(">> Programa reconhecido com sucesso.\n\n--- AST ---\n");
            print_ast(ast_root, 0);
            printf("\n=== Analise concluida sem erros ===\n");
        }
    } else if (!quiet) {
        printf("=== Analise concluida com erros ===\n");
    }

    if (ir_prog) ir_program_free(ir_prog);
    free_ast(ast_root);
    ast_root = NULL;

    if (input_path) {
        fclose(yyin);
    }
    yylex_destroy();

    st_free();
    return success ? 0 : 1;
}
