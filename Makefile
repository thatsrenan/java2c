# Makefile do protótipo Flex + Bison
#
# Disposição dos arquivos:
#   src/main.c         -> ponto de entrada (orquestra a análise)
#   src/lextest.c      -> harness que testa SÓ o lexer (imprime os tokens)
#   src/symbol_table.c -> tabela de símbolos (escopos + análise semântica)
#   parser/parser.y    -> gramática Bison
#   lexer/lexer.l      -> analisador léxico Flex
#   parser/ast.c       -> estruturas e utilitários da AST
#   tests/ast_test.c   -> testes unitários da AST (sem lexer/parser)
#
# Os arquivos gerados por flex/bison (lex.yy.c, parser.tab.c, parser.tab.h)
# são colocados em build/, que é criado automaticamente e não deve ser
# versionado.

SRC_DIR    = src
PARSER_DIR = parser
LEXER_DIR  = lexer
BUILD_DIR  = build

CC     = gcc
CFLAGS = -Wall -Wno-unused-function -I$(BUILD_DIR) -I$(SRC_DIR) -I$(PARSER_DIR)

# Sanitizers usados nos testes da AST: pegam vazamento, double free e acesso
# inválido em free_ast/print_ast. Se o compilador não suportar, rode
# 'make test-ast SANITIZE='.
SANITIZE = -fsanitize=address,undefined -fno-omit-frame-pointer

all: protoparser lextest

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Bison gera parser.tab.c e parser.tab.h a partir de parser/parser.y
$(BUILD_DIR)/parser.tab.c $(BUILD_DIR)/parser.tab.h: $(PARSER_DIR)/parser.y | $(BUILD_DIR)
	bison -d -o $(BUILD_DIR)/parser.tab.c $(PARSER_DIR)/parser.y

# Flex gera lex.yy.c a partir de lexer/lexer.l (depende do header do bison,
# que lexer.l inclui como "parser.tab.h" — resolvido via -I$(BUILD_DIR))
$(BUILD_DIR)/lex.yy.c: $(LEXER_DIR)/lexer.l $(BUILD_DIR)/parser.tab.h
	flex -o $(BUILD_DIR)/lex.yy.c $(LEXER_DIR)/lexer.l

PARSER_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/symbol_table.c $(BUILD_DIR)/parser.tab.c $(BUILD_DIR)/lex.yy.c \
              $(PARSER_DIR)/ast.c

protoparser: $(PARSER_SRCS) $(PARSER_DIR)/ast.h $(SRC_DIR)/symbol_table.h
	$(CC) $(CFLAGS) -o protoparser $(PARSER_SRCS)

# Mesmo protoparser, compilado com os sanitizers: usado por test-ast-parser
# para pegar vazamento ou acesso inválido na montagem/liberação da AST,
# inclusive quando a análise para no meio por erro de sintaxe.
protoparser-san: $(PARSER_SRCS) $(PARSER_DIR)/ast.h $(SRC_DIR)/symbol_table.h
	$(CC) $(CFLAGS) -g $(SANITIZE) -o protoparser-san $(PARSER_SRCS)

# Testa só o lexer: não linka o parser, então funciona mesmo que a
# gramática ainda esteja incompleta.
lextest: $(BUILD_DIR)/lex.yy.c $(SRC_DIR)/lextest.c
	$(CC) $(CFLAGS) -o lextest $(SRC_DIR)/lextest.c $(BUILD_DIR)/lex.yy.c

# Testa só a AST: independe do flex/bison, monta as árvores direto pelos
# construtores new_*.
asttest: tests/ast_test.c $(PARSER_DIR)/ast.c $(PARSER_DIR)/ast.h
	$(CC) $(CFLAGS) -Wextra -g $(SANITIZE) -I$(PARSER_DIR) -o asttest \
		tests/ast_test.c $(PARSER_DIR)/ast.c

test: protoparser
	./protoparser examples/exemplo1.java

# Dump de tokens (um por linha) de cada arquivo em examples/tokens/*.java.
# Serve para conferir "reconhece os tokens definidos" a olho.
test-tokens: lextest
	@for f in examples/tokens/*.java; do \
		echo "===== $$f ====="; \
		./lextest "$$f" 2>&1; \
		echo "(exit code: $$?)"; \
		echo ""; \
	done

# Roda cada caso de examples/lexico/*.java e mostra entrada + saída (stdout+stderr)
# lado a lado. Útil para conferir rapidamente se o lexer/parser reagiu como
# esperado, sem precisar chamar ./protoparser arquivo por arquivo.
test-lexico: protoparser
	@for f in examples/lexico/*.java; do \
		echo "===== $$f ====="; \
		echo "--- entrada ---"; \
		cat "$$f"; \
		echo "--- saída ---"; \
		./protoparser "$$f" 2>&1; \
		echo "(exit code: $$?)"; \
		echo ""; \
	done

# Confere o lexer (via lextest, sem parser) contra o resultado esperado de cada
# caso em examples/lexico/esperado.txt e mostra no terminal quantos casos
# ficaram como esperado. Sai com código 1 se algum divergir.
test-check: lextest
	@bash tests/check.sh ./lextest examples/lexico

# Testes unitários da AST (construtores, append, print_ast, free_ast).
# Sai com código != 0 se algum teste falhar ou o sanitizer acusar erro.
test-ast: asttest
	@./asttest

# Confere a gramática (lexer + parser) contra examples/sintaxe/esperado.txt:
# programas válidos devem ser aceitos e inválidos rejeitados com a mensagem
# esperada. Sai com código 1 se algum divergir.
test-sintaxe: protoparser
	@bash tests/check_parser.sh ./protoparser examples/sintaxe

# Confere a AST montada pelo parser: para cada examples/ast/NOME.java, a
# saída de "protoparser --ast" deve ser idêntica a examples/ast/NOME.ast.
test-ast-parser: protoparser
	@bash tests/check_ast.sh ./protoparser examples/ast

# Roda o parser com sanitizers sobre todos os exemplos (inclusive os
# inválidos, que exercitam os %destructor) e acusa erros de memória.
test-mem: protoparser-san
	@bash tests/check_mem.sh ./protoparser-san

# Confere o parser + tabela de símbolos contra examples/escopo/esperado.txt.
# Um caso 'ok' precisa ser aceito; um caso 'erro' precisa falhar com
# "ERRO SEMANTICO" (e não com erro de sintaxe).
test-escopo-check: protoparser
	@bash tests/check_escopo.sh ./protoparser examples/escopo

# Todas as conferências automáticas que já devem passar.
check: test-check test-ast test-sintaxe test-ast-parser test-mem test-escopo-check

test-escopo: protoparser
	@for f in examples/escopo/*.java; do \
		echo "===== $$f ====="; \
		echo "--- entrada ---"; \
		cat "$$f"; \
		echo "--- saída ---"; \
		./protoparser "$$f" 2>&1; \
		echo "(exit code: $$?)"; \
		echo ""; \
	done

# Roda tudo e salva em arquivos de texto, para revisar depois com um editor
# ou comparar entre execuções (git diff, etc).
test-all-save: protoparser lextest asttest
	@mkdir -p build/resultados
	@$(MAKE) --no-print-directory test-tokens > build/resultados/tokens.txt
	@$(MAKE) --no-print-directory test-lexico > build/resultados/lexico.txt
	@$(MAKE) --no-print-directory test-escopo > build/resultados/escopo.txt
	@$(MAKE) --no-print-directory test-ast > build/resultados/ast.txt 2>&1 || true
	@$(MAKE) --no-print-directory test-ast-parser > build/resultados/ast-parser.txt 2>&1 || true
	@$(MAKE) --no-print-directory test-sintaxe > build/resultados/sintaxe.txt 2>&1 || true
	@echo "Resultados salvos em build/resultados/{tokens,lexico,escopo,ast,ast-parser,sintaxe}.txt"

clean:
	rm -rf protoparser protoparser-san lextest asttest $(BUILD_DIR)

.PHONY: all test test-tokens test-lexico test-check test-ast test-sintaxe test-ast-parser test-mem test-escopo-check check test-escopo test-all-save clean
