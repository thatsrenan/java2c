# API da tabela de símbolos

A tabela de símbolos está em `src/symbol_table.h` / `src/symbol_table.c`.
Ela é uma pilha de escopos; cada escopo tem sua própria tabela hash e a busca
percorre do escopo mais interno ao global.

## Tipos

```c
typedef enum {
    ST_TYPE_INT = 0, ST_TYPE_FLOAT, ST_TYPE_DOUBLE, ST_TYPE_BOOLEAN,
    ST_TYPE_CHAR, ST_TYPE_VOID, ST_TYPE_STRING, ST_TYPE_UNKNOWN
} DataType;

typedef enum {
    SYM_VARIABLE, SYM_PARAMETER, SYM_GLOBAL, SYM_FUNCTION
} SymbolKind;
```

`ST_TYPE_STRING` existe apenas para reconhecer o parâmetro convencional
`String[] args` de `main`. `ST_TYPE_UNKNOWN` evita cascatas de erro quando uma
expressão já inválida é reutilizada. Os prefixos `ST_` distinguem esses
enumeradores dos `TYPE_*` de `parser/ast.h`, que representam os mesmos tipos,
mas com valores numéricos diferentes.

### Estrutura `Symbol`

| Campo | Conteúdo |
| --- | --- |
| `name` | cópia própria do nome |
| `type` | tipo declarado |
| `kind` | variável, parâmetro, global ou função |
| `line` | linha da declaração |
| `scope_level` | `0` global, `1` `main` implícito, demais níveis para métodos/blocos |
| `initialized` | `1` se possui inicializador ou é parâmetro |
| `param_count`, `param_types`, `param_capacity` | assinatura de funções |
| `next` | encadeamento na tabela hash do escopo |

## Ciclo de vida e escopos

| Função | Comportamento |
| --- | --- |
| `void st_init(void)` | cria a tabela com o escopo global; chamar uma vez antes do parsing |
| `void st_free(void)` | libera todos os escopos e símbolos |
| `void st_enter_scope(void)` | entra em bloco, função ou `for` |
| `void st_enter_function_scope(void)` | entra no escopo de método: só o global é visível, não os locais do `main` implícito |
| `void st_exit_scope(void)` | sai do escopo atual; o global não é removido |
| `int st_scope_level(void)` | nível do escopo atual (`0` = global) |
| `int st_is_global_scope(void)` | `1` se o escopo atual é o global |

## Inserção e consulta

| Função | Retorno |
| --- | --- |
| `Symbol *st_insert(const char *name, DataType type, SymbolKind kind, int line)` | símbolo inserido no escopo atual ou `NULL` se já existir nome igual nesse escopo |
| `Symbol *st_insert_global(...)` | o mesmo, mas diretamente no escopo global (campos `static` e funções) |
| `Symbol *st_lookup(const char *name)` | símbolo do escopo mais interno para o mais externo, ou `NULL` |
| `Symbol *st_lookup_global(const char *name)` | somente no escopo global |
| `Symbol *st_lookup_current_scope(const char *name)` | somente no escopo atual |
| `Symbol *st_lookup_local_visible(const char *name)` | qualquer escopo local visível, exceto o global; usado para impedir ocultação de local por local |

## Símbolos e saída

| Função | Efeito |
| --- | --- |
| `void st_add_parameter(Symbol *function, DataType type)` | acrescenta um tipo à assinatura da função |
| `void st_mark_initialized(Symbol *symbol)` | marca variável/parâmetro como inicializado |
| `const char *st_type_name(DataType type)` | nome legível do tipo |
| `const char *st_kind_name(SymbolKind kind)` | nome legível da categoria |
| `void st_print(FILE *out)` | imprime os escopos ativos, do mais interno ao global; usado por `--tabela` |

## Regras implementadas com a API

O parser e a análise de fluxo usam a tabela para verificar:

- uso de variável ou função não declarada;
- redeclaração no mesmo escopo e ocultação de local visível (locais podem
  ocultar globais);
- visibilidade restrita de variáveis de `for` e de blocos;
- declaração de função antes da chamada e proibição de sobrecarga;
- compatibilidade de tipos em atribuições, inicializações, retornos e
  operadores;
- `break`/`continue` somente dentro de laços.

Erros são emitidos como `ERRO SEMANTICO na linha N: <descrição>`. Regras que
dependem do programa inteiro — retorno em todos os caminhos, código
inalcançável e atribuição definitiva — ficam em
[`semantic_flow_check`](../reference/ir-api.md#analise-de-fluxo-e-constantes)
e são descritas em [Tabela de símbolos](../architecture/symbol-table.md).
