/*
 * symbol_table.c
 *
 * Implementacao da tabela de simbolos com escopos encadeados. Cada escopo
 * tem sua propria tabela hash (encadeamento separado), o que torna trivial
 * descartar todos os simbolos de um bloco ao sair dele: basta liberar a
 * lista de simbolos daquele escopo.
 */

#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>

#define ST_BUCKETS 211

typedef struct Scope {
    Symbol       *buckets[ST_BUCKETS];
    struct Scope *parent;   /* escopo onde a busca continua (cadeia lexica) */
    struct Scope *previous; /* escopo ativo antes de entrar neste (para sair) */
    int           level;
} Scope;

static Scope *global_scope = NULL;
static Scope *current_scope = NULL;

/* djb2: hash simples e estavel para nomes de identificadores. */
static unsigned int st_hash(const char *name) {
    unsigned int h = 5381u;
    const unsigned char *p = (const unsigned char *) name;
    while (*p) {
        h = ((h << 5) + h) + *p;
        p++;
    }
    return h % ST_BUCKETS;
}

static Symbol *find_in_scope(Scope *scope, const char *name) {
    if (!scope) return NULL;
    Symbol *s = scope->buckets[st_hash(name)];
    for (; s != NULL; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

static Scope *scope_new(Scope *parent, Scope *previous, int level) {
    Scope *scope = (Scope *) calloc(1, sizeof *scope);
    if (!scope) {
        fprintf(stderr, "ERRO INTERNO: memoria insuficiente para novo escopo.\n");
        exit(EXIT_FAILURE);
    }
    scope->parent = parent;
    scope->previous = previous;
    scope->level = level;
    return scope;
}

static void symbol_free(Symbol *symbol) {
    if (!symbol) return;
    free(symbol->name);
    free(symbol->param_types);
    free(symbol);
}

static void scope_free(Scope *scope) {
    if (!scope) return;
    for (int i = 0; i < ST_BUCKETS; i++) {
        Symbol *s = scope->buckets[i];
        while (s) {
            Symbol *next = s->next;
            symbol_free(s);
            s = next;
        }
    }
    free(scope);
}

static Symbol *insert_into(Scope *scope, const char *name, DataType type,
                           SymbolKind kind, int line) {
    if (!scope || find_in_scope(scope, name)) return NULL;

    Symbol *symbol = (Symbol *) calloc(1, sizeof *symbol);
    if (!symbol) {
        fprintf(stderr, "ERRO INTERNO: memoria insuficiente para novo simbolo.\n");
        exit(EXIT_FAILURE);
    }

    symbol->name = strdup(name);
    if (!symbol->name) {
        fprintf(stderr, "ERRO INTERNO: memoria insuficiente para nome de simbolo.\n");
        free(symbol);
        exit(EXIT_FAILURE);
    }

    symbol->type = type;
    symbol->kind = kind;
    symbol->line = line;
    symbol->scope_level = scope->level;
    symbol->initialized = 0;
    symbol->param_count = 0;
    symbol->param_types = NULL;
    symbol->param_capacity = 0;

    unsigned int bucket = st_hash(name);
    symbol->next = scope->buckets[bucket];
    scope->buckets[bucket] = symbol;
    return symbol;
}

void st_init(void) {
    st_free();
    global_scope = scope_new(NULL, NULL, 0);
    current_scope = global_scope;
}

void st_free(void) {
    while (current_scope && current_scope != global_scope) {
        Scope *previous = current_scope->previous;
        scope_free(current_scope);
        current_scope = previous;
    }
    scope_free(global_scope);
    global_scope = NULL;
    current_scope = NULL;
}

void st_enter_scope(void) {
    if (!global_scope) st_init();
    int level = current_scope ? current_scope->level + 1 : 0;
    current_scope = scope_new(current_scope, current_scope, level);
}

void st_enter_function_scope(void) {
    if (!global_scope) st_init();
    int level = current_scope ? current_scope->level + 1 : 1;
    /* O corpo de um metodo nao enxerga variaveis locais do main implicito:
     * a busca continua no escopo global, mas ao sair voltamos ao escopo
     * que estava ativo antes (previous). */
    current_scope = scope_new(global_scope, current_scope, level);
}

void st_exit_scope(void) {
    if (!current_scope || current_scope == global_scope) {
        return;
    }
    Scope *previous = current_scope->previous;
    scope_free(current_scope);
    current_scope = previous;
}

int st_scope_level(void) {
    return current_scope ? current_scope->level : 0;
}

int st_is_global_scope(void) {
    return current_scope == global_scope;
}

Symbol *st_insert(const char *name, DataType type, SymbolKind kind, int line) {
    if (!global_scope) st_init();
    return insert_into(current_scope, name, type, kind, line);
}

Symbol *st_insert_global(const char *name, DataType type, SymbolKind kind, int line) {
    if (!global_scope) st_init();
    return insert_into(global_scope, name, type, kind, line);
}

Symbol *st_lookup(const char *name) {
    for (Scope *scope = current_scope; scope != NULL; scope = scope->parent) {
        Symbol *s = find_in_scope(scope, name);
        if (s) return s;
    }
    return NULL;
}

Symbol *st_lookup_global(const char *name) {
    return find_in_scope(global_scope, name);
}

Symbol *st_lookup_current_scope(const char *name) {
    return find_in_scope(current_scope, name);
}

Symbol *st_lookup_local_visible(const char *name) {
    for (Scope *scope = current_scope;
         scope != NULL && scope != global_scope;
         scope = scope->parent) {
        Symbol *s = find_in_scope(scope, name);
        if (s) return s;
    }
    return NULL;
}

void st_add_parameter(Symbol *function, DataType type) {
    if (!function || function->kind != SYM_FUNCTION) return;

    if (function->param_count == function->param_capacity) {
        int new_capacity = function->param_capacity == 0 ? 4 : function->param_capacity * 2;
        DataType *new_types = (DataType *) realloc(function->param_types,
                                                   (size_t) new_capacity * sizeof(DataType));
        if (!new_types) {
            fprintf(stderr, "ERRO INTERNO: memoria insuficiente para assinatura de funcao.\n");
            exit(EXIT_FAILURE);
        }
        function->param_types = new_types;
        function->param_capacity = new_capacity;
    }

    function->param_types[function->param_count++] = type;
}

void st_mark_initialized(Symbol *symbol) {
    if (symbol) symbol->initialized = 1;
}

const char *st_type_name(DataType type) {
    switch (type) {
    case TYPE_INT:     return "int";
    case TYPE_FLOAT:   return "float";
    case TYPE_DOUBLE:  return "double";
    case TYPE_BOOLEAN: return "boolean";
    case TYPE_CHAR:    return "char";
    case TYPE_VOID:    return "void";
    case TYPE_STRING:  return "String";
    default:           return "desconhecido";
    }
}

const char *st_kind_name(SymbolKind kind) {
    switch (kind) {
    case SYM_VARIABLE:  return "variavel";
    case SYM_PARAMETER: return "parametro";
    case SYM_GLOBAL:    return "global";
    case SYM_FUNCTION:  return "funcao";
    default:            return "?";
    }
}

static void print_symbol(FILE *out, const Symbol *symbol) {
    fprintf(out, "  %-16s %-10s %-8s linha %-4d escopo %d",
            symbol->name, st_kind_name(symbol->kind),
            st_type_name(symbol->type), symbol->line, symbol->scope_level);

    if (symbol->kind == SYM_FUNCTION) {
        fprintf(out, "  (");
        for (int i = 0; i < symbol->param_count; i++) {
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "%s", st_type_name(symbol->param_types[i]));
        }
        fprintf(out, ")");
    }

    if (symbol->initialized) fprintf(out, " [inicializado]");
    fputc('\n', out);
}

void st_print(FILE *out) {
    if (!out) out = stdout;

    fprintf(out, "\n=== Tabela de simbolos ===\n");
    fprintf(out, "  %-16s %-10s %-8s %-10s %s\n",
            "NOME", "CATEGORIA", "TIPO", "LINHA", "ESCOPO");

    if (!current_scope) {
        fprintf(out, "  (vazia)\n");
        return;
    }

    for (Scope *scope = current_scope; scope != NULL; scope = scope->parent) {
        if (scope->level == 0) {
            fprintf(out, "  -- escopo global --\n");
        } else {
            fprintf(out, "  -- escopo local nivel %d --\n", scope->level);
        }

        int empty = 1;
        for (int i = 0; i < ST_BUCKETS; i++) {
            for (Symbol *s = scope->buckets[i]; s != NULL; s = s->next) {
                print_symbol(out, s);
                empty = 0;
            }
        }
        if (empty) fprintf(out, "  (vazio)\n");
    }
}
