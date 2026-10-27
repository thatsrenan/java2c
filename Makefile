# Makefile do protótipo Flex + Bison
#
# Disposição dos arquivos:
#   src/main.c       -> ponto de entrada (orquestra a análise)
#   src/lextest.c    -> harness que testa SÓ o lexer (imprime os tokens)
#   parser/parser.y  -> gramática Bison
#   lexer/lexer.l     -> analisador léxico Flex
#   parser/ast.c     -> estruturas e utilitarios da AST
#   tests/ast_test.c -> testes unitarios da AST (sem lexer/parser)
#
# Os arquivos gerados por flex/bison (lex.yy.c, parser.tab.c, parser.tab.h)
# são colocados em build/, que é criado automaticamente e não deve ser
# versionado.

SRC_DIR    = src
PARSER_DIR = parser
LEXER_DIR  = lexer
BUILD_DIR  = build
 
CC     = gcc
CFLAGS = -Wall -Wno-unused-function -I$(BUILD_DIR) -I$(PARSER_DIR)

# Sanitizers usados nos testes da AST: pegam vazamento, double free e acesso
# invalido em free_ast/print_ast. Se o compilador nao suportar, rode
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
 
PARSER_SRCS = $(SRC_DIR)/main.c $(BUILD_DIR)/parser.tab.c $(BUILD_DIR)/lex.yy.c \
              $(PARSER_DIR)/ast.c

protoparser: $(PARSER_SRCS) $(PARSER_DIR)/ast.h
	$(CC) $(CFLAGS) -o protoparser $(PARSER_SRCS)

# Mesmo protoparser, compilado com os sanitizers: usado por test-ast-parser
# para pegar vazamento ou acesso invalido na montagem/liberacao da AST,
# inclusive quando a analise para no meio por erro de sintaxe.
protoparser-san: $(PARSER_SRCS) $(PARSER_DIR)/ast.h
	$(CC) $(CFLAGS) -g $(SANITIZE) -o protoparser-san $(PARSER_SRCS)
 
# Testa só o lexer: não linka o parser, então funciona mesmo que a
# gramática ainda esteja incompleta.
lextest: $(BUILD_DIR)/lex.yy.c $(SRC_DIR)/lextest.c
	$(CC) $(CFLAGS) -o lextest $(SRC_DIR)/lextest.c $(BUILD_DIR)/lex.yy.c

# Testa so a AST: independe do flex/bison, monta as arvores direto pelos
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
 
# Roda cada caso de examples/lexico/*.java e mostra entrada + saida (stdout+stderr)
# lado a lado. Util para conferir rapidamente se o lexer/parser reagiu como
# esperado, sem precisar chamar ./protoparser arquivo por arquivo.
test-lexico: protoparser
	@for f in examples/lexico/*.java; do \
		echo "===== $$f ====="; \
		echo "--- entrada ---"; \
		cat "$$f"; \
		echo "--- saida ---"; \
		./protoparser "$$f" 2>&1; \
		echo "(exit code: $$?)"; \
		echo ""; \
	done
 
# Confere o lexer (via lextest, sem parser) contra o resultado esperado de cada
# caso em examples/lexico/esperado.txt e mostra no terminal quantos casos
# ficaram como esperado. Sai com codigo 1 se algum divergir.
test-check: lextest
	@bash tests/check.sh ./lextest examples/lexico

# Testes unitarios da AST (construtores, append, print_ast, free_ast).
# Sai com codigo != 0 se algum teste falhar ou o sanitizer acusar erro.
test-ast: asttest
	@./asttest

# Confere a gramatica (lexer + parser) contra examples/sintaxe/esperado.txt:
# programas validos devem ser aceitos e invalidos rejeitados com a mensagem
# esperada. Sai com codigo 1 se algum divergir.
test-sintaxe: protoparser
	@bash tests/check_parser.sh ./protoparser examples/sintaxe

# Confere a AST montada pelo parser: para cada examples/ast/NOME.java, a
# saida de "protoparser --ast" deve ser identica a examples/ast/NOME.ast.
test-ast-parser: protoparser
	@bash tests/check_ast.sh ./protoparser examples/ast

# Roda o parser com sanitizers sobre todos os exemplos (inclusive os
# invalidos, que exercitam os %destructor) e acusa erros de memoria.
test-mem: protoparser-san
	@bash tests/check_mem.sh ./protoparser-san

# Confere examples/escopo/esperado.txt. Os casos de erro dependem da analise
# semantica, que ainda nao existe, por isso este alvo ainda falha e fica
# fora do 'check'.
test-escopo-check: protoparser
	@bash tests/check_parser.sh ./protoparser examples/escopo

# Todas as conferencias automaticas que ja devem passar.
check: test-check test-ast test-sintaxe test-ast-parser test-mem

# Mesma ideia para examples/escopo/*.java (so mostra as saidas; a conferencia
# automatica e o alvo test-escopo-check). O prototipo ainda nao faz checagem
# de escopo/semantica, entao isso exercita apenas a gramatica.
test-escopo: protoparser
	@for f in examples/escopo/*.java; do \
		echo "===== $$f ====="; \
		echo "--- entrada ---"; \
		cat "$$f"; \
		echo "--- saida ---"; \
		./protoparser "$$f" 2>&1; \
		echo "(exit code: $$?)"; \
		echo ""; \
	done
 
# Roda tudo e salva em arquivos de texto, para revisar depois com um editor
# ou comparar entre execucoes (git diff, etc).
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