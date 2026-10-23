/*
 * symbol_table.h
 *
 * Tabela de simbolos do compilador Java -> C.
 *
 * A tabela e organizada em escopos encadeados (uma pilha de escopos). Cada
 * escopo possui sua propria tabela hash; a busca por um nome percorre do
 * escopo mais interno para o mais externo. Isso implementa diretamente as
 * regras de escopo do subconjunto:
 *
 *   - a variavel local so existe dentro do bloco onde foi declarada;
 *   - parametros pertencem ao escopo do metodo;
 *   - campos static pertencem ao escopo global;
 *   - ao sair de um bloco, todos os simbolos declarados nele sao descartados.
 *
 * O escopo global (nivel 0) guarda campos static e funcoes. O parser cria um
 * escopo "main" implicito (nivel 1) para os comandos soltos do programa, de
 * modo que variaveis declaradas no topo se comportem como locais.
 */

#ifndef JAVA2C_SYMBOL_TABLE_H
#define JAVA2C_SYMBOL_TABLE_H

#include <stdio.h>

/* Tipos do subconjunto. TYPE_STRING existe apenas para reconhecer o
 * parametro convencional "String[] args" do main, que depois nao pode ser
 * usado como um tipo comum. TYPE_UNKNOWN evita cascatas de erro quando uma
 * expressao ja invalida e usada em outra verificacao. */
typedef enum {
    TYPE_INT = 0,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BOOLEAN,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_STRING,
    TYPE_UNKNOWN
} DataType;

/* Categoria do simbolo armazenado. */
typedef enum {
    SYM_VARIABLE,   /* variavel local */
    SYM_PARAMETER,  /* parametro de funcao */
    SYM_GLOBAL,     /* campo static / variavel global */
    SYM_FUNCTION    /* metodo static */
} SymbolKind;

typedef struct Symbol {
    char      *name;        /* nome do simbolo (copia propria) */
    DataType   type;        /* tipo declarado */
    SymbolKind kind;        /* categoria */
    int        line;        /* linha da declaracao no fonte */
    int        scope_level; /* 0 = global, 1 = main implicito, ... */
    int        initialized; /* 1 se possui inicializador/parametro */
    int        param_count; /* quantidade de parametros (funcoes) */
    DataType  *param_types; /* tipos dos parametros (funcoes) */
    int        param_capacity;
    struct Symbol *next;    /* encadeamento na tabela hash do escopo */
} Symbol;

/* Cria a tabela com o escopo global. Deve ser chamada uma vez antes do parse. */
void st_init(void);

/* Libera todos os escopos e simbolos. */
void st_free(void);

/* Entra em um novo escopo (bloco, funcao, for). */
void st_enter_scope(void);

/* Entra no escopo de um metodo. Parametros e locais do metodo NAO enxergam
 * variaveis locais do main implicito; apenas o escopo global e visivel. */
void st_enter_function_scope(void);

/* Sai do escopo atual. Nao remove o escopo global. */
void st_exit_scope(void);

/* Nivel do escopo atual (0 = global). */
int st_scope_level(void);

/* 1 se o escopo atual e o global. */
int st_is_global_scope(void);

/* Insere no escopo atual. Retorna NULL se ja existir o mesmo nome no
 * escopo atual (redeclaracao). */
Symbol *st_insert(const char *name, DataType type, SymbolKind kind, int line);

/* Insere diretamente no escopo global (campos static e funcoes). Retorna
 * NULL se ja existir o mesmo nome no escopo global. */
Symbol *st_insert_global(const char *name, DataType type, SymbolKind kind, int line);

/* Busca do escopo mais interno para o mais externo. */
Symbol *st_lookup(const char *name);

/* Busca somente no escopo global. */
Symbol *st_lookup_global(const char *name);

/* Busca somente no escopo atual. */
Symbol *st_lookup_current_scope(const char *name);

/* Busca um nome em qualquer escopo local visivel (exclui o global). Serve
 * para impedir que uma variavel local oculte outra variavel local visivel. */
Symbol *st_lookup_local_visible(const char *name);

/* Registra o tipo de um parametro na assinatura da funcao. */
void st_add_parameter(Symbol *function, DataType type);

/* Marca uma variavel/parametro como inicializado. */
void st_mark_initialized(Symbol *symbol);

/* Nome legivel do tipo (ex.: "int", "boolean", "String"). */
const char *st_type_name(DataType type);

/* Nome legivel da categoria (ex.: "variavel", "funcao"). */
const char *st_kind_name(SymbolKind kind);

/* Imprime todos os escopos atualmente ativos (do mais interno ao global).
 * Util para depuracao e para a opcao --tabela do executavel. */
void st_print(FILE *out);

#endif /* JAVA2C_SYMBOL_TABLE_H */
