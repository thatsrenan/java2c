# Makefile do protótipo Flex + Bison
#
# Disposição dos arquivos:
#   src/main.c       -> ponto de entrada (orquestra a análise)
#   parser/parser.y  -> gramática Bison
#   lexer/lexer.l     -> analisador léxico Flex
#
# Os arquivos gerados por flex/bison (lex.yy.c, parser.tab.c, parser.tab.h)
# são colocados em build/, que é criado automaticamente e não deve ser
# versionado.

SRC_DIR    = src
PARSER_DIR = parser
LEXER_DIR  = lexer
BUILD_DIR  = build

CC     = gcc
CFLAGS = -Wall -Wno-unused-function -I$(BUILD_DIR)

all: protoparser

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Bison gera parser.tab.c e parser.tab.h a partir de parser/parser.y
$(BUILD_DIR)/parser.tab.c $(BUILD_DIR)/parser.tab.h: $(PARSER_DIR)/parser.y | $(BUILD_DIR)
	bison -d -o $(BUILD_DIR)/parser.tab.c $(PARSER_DIR)/parser.y

# Flex gera lex.yy.c a partir de lexer/lexer.l (depende do header do bison,
# que lexer.l inclui como "parser.tab.h" — resolvido via -I$(BUILD_DIR))
$(BUILD_DIR)/lex.yy.c: $(LEXER_DIR)/lexer.l $(BUILD_DIR)/parser.tab.h
	flex -o $(BUILD_DIR)/lex.yy.c $(LEXER_DIR)/lexer.l

protoparser: $(BUILD_DIR)/parser.tab.c $(BUILD_DIR)/lex.yy.c $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -o protoparser \
		$(SRC_DIR)/main.c \
		$(BUILD_DIR)/parser.tab.c \
		$(BUILD_DIR)/lex.yy.c

test: protoparser
	./protoparser examples/exemplo1.java

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

# Mesma ideia para examples/escopo/*.java. Hoje isso so exercita o parser
# estrutural (o prototipo ainda nao faz checagem de escopo/semantica), mas
# ja serve para ver quais casos passam ou nao pela gramatica atual.
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
test-all-save: protoparser
	@mkdir -p build/resultados
	@$(MAKE) --no-print-directory test-lexico > build/resultados/lexico.txt
	@$(MAKE) --no-print-directory test-escopo > build/resultados/escopo.txt
	@echo "Resultados salvos em build/resultados/lexico.txt e build/resultados/escopo.txt"

clean:
	rm -rf protoparser $(BUILD_DIR)

.PHONY: all test clean