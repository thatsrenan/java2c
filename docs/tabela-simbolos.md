# Tabela de Símbolos

Este documento descreve a tabela de símbolos do compilador Java → C:
as estruturas de dados usadas, o modelo de escopos, a lógica de inserção e
busca, a API disponível e como o analisador sintático (`parser/parser.y`) usa
essa tabela para realizar as verificações de escopo e de declaração.

Arquivos envolvidos:

| Arquivo | Responsabilidade |
| --- | --- |
| `src/symbol_table.h` | Tipos, estruturas e API pública da tabela |
| `src/symbol_table.c` | Implementação dos escopos, tabela hash e operações |
| `parser/parser.y` | Cria/consulta símbolos e reporta erros semânticos |
| `src/main.c` | Inicializa/libera a tabela e define o status final da compilação |

---

## 1. Visão geral

A tabela de símbolos é o componente que guarda informações sobre os nomes
declarados no programa:

- variáveis locais;
- parâmetros de métodos;
- campos `static` (variáveis globais em C);
- métodos `static` (funções em C).

Ela é usada em duas direções:

1. **Declaração:** ao encontrar uma declaração, o parser insere um símbolo no
   escopo apropriado.
2. **Uso:** ao encontrar um identificador em uma expressão, atribuição,
   chamada etc., o parser consulta a tabela para verificar se o nome existe,
   qual é o seu tipo e se ele pode ser usado naquele contexto.

A organização é uma **pilha de escopos encadeados**. Cada escopo possui sua
própria tabela hash. A busca por um nome percorre do escopo mais interno para
o mais externo, implementando a regra lexical de escopo do Java.

```text
+---------------------------+
| Escopo do bloco interno   |  <- escopo atual
+---------------------------+
            | parent
            v
+---------------------------+
| Escopo do método / função |  <- parâmetros
+---------------------------+
            | parent
            v
+---------------------------+
| Escopo global             |  <- campos static e funções
+---------------------------+
```

Ao sair de um escopo, todos os símbolos declarados nele são liberados. Assim,
não é necessário remover nome por nome.

---

## 2. Estruturas de dados

### 2.1 `DataType`

Representa os tipos do subconjunto:

```c
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_BOOLEAN,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_STRING,
    TYPE_UNKNOWN
} DataType;
```

| Tipo | Uso |
| --- | --- |
| `TYPE_INT` | `int` |
| `TYPE_FLOAT` | `float` |
| `TYPE_DOUBLE` | `double` |
| `TYPE_BOOLEAN` | `boolean` (traduzido para `int` em C) |
| `TYPE_CHAR` | `char` |
| `TYPE_VOID` | retorno de método |
| `TYPE_STRING` | apenas para reconhecer `String[] args` do `main` |
| `TYPE_UNKNOWN` | usado para evitar cascatas de erro após uma falha |

`TYPE_STRING` não representa um tipo válido para variáveis ou parâmetros
comuns. Ele existe para que o parâmetro convencional `String[] args` do
`main` possa ser registrado. `TYPE_UNKNOWN` faz com que verificações
posteriores não gerem uma sequência de mensagens em cima de um erro já
reportado.

### 2.2 `SymbolKind`

Indica a categoria do símbolo:

```c
typedef enum {
    SYM_VARIABLE,   /* variável local */
    SYM_PARAMETER,  /* parâmetro de função */
    SYM_GLOBAL,     /* campo static / variável global */
    SYM_FUNCTION    /* método static */
} SymbolKind;
```

A categoria influencia as regras de inserção. Por exemplo, uma variável local
pode ocultar um símbolo global, mas não pode ocultar outra variável local
visível.

### 2.3 `Symbol`

Cada entrada da tabela é um `Symbol`:

```c
typedef struct Symbol {
    char      *name;
    DataType   type;
    SymbolKind kind;
    int        line;
    int        scope_level;
    int        initialized;
    int        param_count;
    DataType  *param_types;
    int        param_capacity;
    struct Symbol *next;
} Symbol;
```

| Campo | Significado |
| --- | --- |
| `name` | Nome do símbolo (cópia própria, liberada no fim) |
| `type` | Tipo declarado |
| `kind` | Categoria do símbolo |
| `line` | Linha da declaração no código-fonte |
| `scope_level` | Nível do escopo: `0` global, `1` main implícito etc. |
| `initialized` | Indica se possui inicializador ou é parâmetro |
| `param_count` | Quantidade de parâmetros (apenas funções) |
| `param_types` | Vetor com os tipos dos parâmetros (apenas funções) |
| `param_capacity` | Capacidade alocada do vetor de parâmetros |
| `next` | Próximo símbolo na mesma posição da tabela hash |

### 2.4 `Scope`

Cada escopo tem sua própria tabela hash:

```c
typedef struct Scope {
    Symbol       *buckets[ST_BUCKETS];
    struct Scope *parent;
    struct Scope *previous;
    int           level;
} Scope;
```

| Campo | Significado |
| --- | --- |
| `buckets` | Vetor de listas encadeadas da tabela hash do escopo |
| `parent` | Escopo onde a busca continua (cadeia lexical) |
| `previous` | Escopo que estava ativo antes deste (para restauração ao sair) |
| `level` | Nível numérico do escopo |

A distinção entre `parent` e `previous` é importante:

- `parent` define **quem é visível** durante a busca.
- `previous` define **para onde voltar** ao sair do escopo.

O caso mais claro é o escopo de método:

- o `parent` de um método é o **escopo global**, para que o corpo do método
  veja campos `static` e funções, mas **não** veja variáveis locais do `main`
  implícito;
- o `previous` aponta para o escopo do `main` implícito, para que, ao terminar
  o método, a análise continue exatamente de onde estava.

---

## 3. Modelo de escopos

O parser utiliza os seguintes escopos:

| Nível | Escopo | Conteúdo |
| --- | --- | --- |
| `0` | Global | Campos `static` e métodos `static` |
| `1` | `main` implícito | Comandos soltos no topo do arquivo |
| `2+` | Método | Parâmetros do método |
| `3+` | Bloco / `for` | Variáveis locais |

### 3.1 Escopo global (nível 0)

Criado em `st_init()`. Guarda:

- campos `static`, que serão traduzidos para variáveis globais em C;
- funções (`static`), que serão traduzidas para funções C.

Esses símbolos são inseridos com `st_insert_global()`, independentemente do
escopo ativo no momento da declaração.

### 3.2 Escopo `main` implícito (nível 1)

O parser aceita fragmentos de código com comandos soltos no topo do arquivo,
como nos exemplos de `examples/escopo/`:

```java
int x = 10;
System.out.println(x);
```

Para que esses comandos se comportem como código dentro de um método, o
parser entra em um escopo implícito antes de analisar a lista de declarações:

```c
program:
    { st_enter_scope(); }
    top_list
    {
        if (show_symbol_table) st_print(stdout);
        st_exit_scope();
    }
;
```

Assim, `int x` no topo é uma variável **local** do `main` implícito, e não um
campo global. Isso é o que permite, por exemplo, que o caso `E20` detecte que
a variável `i` do `for` oculta outra variável local visível.

### 3.3 Escopo de método

Ao reconhecer um método, o parser:

1. insere a função no escopo global;
2. entra em um escopo de método com `st_enter_function_scope()`;
3. registra os parâmetros nesse escopo;
4. analisa o corpo do método;
5. sai do escopo, descartando parâmetros.

```c
method_decl:
    opt_public STATIC type IDENTIFIER
    {
        Symbol *function = declare_function($4, $3, yylineno);
        current_function = function;
        current_return_type = $3;
        st_enter_function_scope();
    }
    LPAREN param_list_opt RPAREN block
    {
        st_exit_scope();
        current_function = NULL;
        current_return_type = TYPE_VOID;
    }
;
```

O escopo do método tem `parent = global`, mas `previous = escopo atual`.
Por isso, métodos diferentes não compartilham parâmetros ou variáveis locais,
e um método não enxerga variáveis locais do `main` implícito.

### 3.4 Escopo de bloco

Todo bloco `{ ... }` abre um novo escopo:

```c
block:
    LBRACE
    { st_enter_scope(); }
    stmt_list
    RBRACE
    { st_exit_scope(); }
;
```

Consequências:

- uma variável declarada no bloco não é visível fora dele;
- blocos irmãos podem declarar o mesmo nome sem conflito;
- ao sair do bloco, a tabela hash daquele escopo é liberada inteira.

### 3.5 Escopo de `for`

O `for` abre um escopo próprio antes da inicialização e o fecha após o corpo:

```c
for_stmt:
    FOR LPAREN
    { st_enter_scope(); }
    for_init SEMICOLON for_cond SEMICOLON for_update RPAREN
    { loop_depth++; }
    block
    {
        loop_depth--;
        st_exit_scope();
    }
;
```

Assim, a variável declarada em `for (int i = 0; ...)` existe apenas dentro do
`for` (incluindo condição, atualização e corpo), e não depois dele.

---

## 4. Tabela hash

Cada escopo usa uma tabela hash com **211 buckets** (`ST_BUCKETS`) e
tratamento de colisões por **encadeamento separado**.

### 4.1 Função de hash

A função utilizada é a djb2:

```c
static unsigned int st_hash(const char *name) {
    unsigned int h = 5381u;
    const unsigned char *p = (const unsigned char *) name;
    while (*p) {
        h = ((h << 5) + h) + *p;
        p++;
    }
    return h % ST_BUCKETS;
}
```

Ela é determinística e barata, adequada para nomes de identificadores.

### 4.2 Inserção

A inserção sempre ocorre no **início** da lista do bucket:

```c
unsigned int bucket = st_hash(name);
symbol->next = scope->buckets[bucket];
scope->buckets[bucket] = symbol;
```

Antes de inserir, `insert_into()` verifica se o nome já existe **naquele
escopo**. Se existir, retorna `NULL`, sinalizando redeclaração:

```c
if (!scope || find_in_scope(scope, name)) return NULL;
```

### 4.3 Busca

A busca em um escopo percorre a lista do bucket correspondente:

```c
static Symbol *find_in_scope(Scope *scope, const char *name) {
    Symbol *s = scope->buckets[st_hash(name)];
    for (; s != NULL; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}
```

A busca lexical completa percorre a cadeia de `parent`:

```c
Symbol *st_lookup(const char *name) {
    for (Scope *scope = current_scope; scope != NULL; scope = scope->parent) {
        Symbol *s = find_in_scope(scope, name);
        if (s) return s;
    }
    return NULL;
}
```

A primeira ocorrência encontrada é a que vale, pois a busca começa no escopo
mais interno. Isso implementa o sombreamento (*shadowing*) de forma natural.

### 4.4 Liberação

Ao sair de um escopo, todos os símbolos das listas de todos os buckets são
liberados:

```c
static void scope_free(Scope *scope) {
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
```

Como cada escopo tem sua própria tabela, não é necessário remover símbolos
individualmente nem manter um histórico de nomes declarados no bloco.

---

## 5. API da tabela

A API pública está em `src/symbol_table.h`.

### 5.1 Ciclo de vida

| Função | Descrição |
| --- | --- |
| `void st_init(void)` | Cria o escopo global e define o escopo atual |
| `void st_free(void)` | Libera todos os escopos e símbolos |

`st_init()` é chamada em `main()` antes de `yyparse()`; `st_free()` é chamada
depois da análise.

### 5.2 Entrada e saída de escopos

| Função | Descrição |
| --- | --- |
| `void st_enter_scope(void)` | Entra em um escopo comum (bloco, `for`) |
| `void st_enter_function_scope(void)` | Entra em um escopo de método |
| `void st_exit_scope(void)` | Sai do escopo atual; não remove o global |

### 5.3 Inserção

| Função | Descrição |
| --- | --- |
| `Symbol *st_insert(...)` | Insere no escopo atual |
| `Symbol *st_insert_global(...)` | Insere diretamente no escopo global |

Ambas retornam `NULL` quando o nome já existe no escopo de destino.

### 5.4 Busca

| Função | Descrição |
| --- | --- |
| `Symbol *st_lookup(name)` | Busca do escopo mais interno para o global |
| `Symbol *st_lookup_global(name)` | Busca apenas no escopo global |
| `Symbol *st_lookup_current_scope(name)` | Busca apenas no escopo atual |
| `Symbol *st_lookup_local_visible(name)` | Busca em escopos locais visíveis, excluindo o global |

`st_lookup_local_visible()` é usada para impedir que uma variável local oculte
outra variável local visível, mas permitir que ela oculte uma global.

### 5.5 Utilitários

| Função | Descrição |
| --- | --- |
| `void st_add_parameter(Symbol *fn, DataType type)` | Adiciona um tipo à assinatura da função |
| `void st_mark_initialized(Symbol *s)` | Marca variável/parâmetro como inicializado |
| `const char *st_type_name(DataType t)` | Nome legível do tipo |
| `const char *st_kind_name(SymbolKind k)` | Nome legível da categoria |
| `void st_print(FILE *out)` | Imprime os escopos ativos (opção `--tabela`) |

---

## 6. Lógica de declaração no parser

O parser não insere símbolos diretamente em todos os casos. Ele usa funções
auxiliares que aplicam as regras semânticas antes da inserção.

### 6.1 Declaração de variável

`declare_variable()`:

1. rejeita `void` e `String` como tipo de variável;
2. decide a categoria:
   - `SYM_GLOBAL` se estiver no escopo global;
   - `SYM_VARIABLE` caso contrário;
3. chama `declare_symbol()`, que aplica a regra de sombreamento;
4. marca como inicializada quando há inicializador.

Regra de sombreamento para variável local:

```c
if (st_lookup_local_visible(name) != NULL) {
    semantic_error("a variavel '%s' ja foi declarada neste escopo", name);
    return NULL;
}
```

Isso cobre três situações:

- redeclaração no mesmo bloco;
- variável local ocultando um parâmetro;
- variável de `for` ocultando uma variável local já visível.

Como a busca exclui o escopo global, uma variável local **pode** ocultar um
campo `static` global.

### 6.2 Declaração de campo global

`declare_global()`:

1. rejeita `void` e `String`;
2. insere no escopo global com `st_insert_global()`;
3. se o nome já existir no global, reporta erro.

### 6.3 Declaração de função

`declare_function()`:

1. rejeita retorno `String`;
2. insere a função no escopo global;
3. se já existir uma função com o mesmo nome, reporta erro de sobrecarga.

A assinatura é montada aos poucos: cada parâmetro é declarado no escopo do
método e, em seguida, `st_add_parameter()` registra seu tipo na função.

### 6.4 Declaração de parâmetro

`declare_parameter()`:

1. rejeita `void`;
2. rejeita `String`, exceto para `String[] args` do `main`;
3. insere no escopo do método;
4. adiciona o tipo à assinatura da função;
5. marca o parâmetro como inicializado.

---

## 7. Lógica de uso no parser

### 7.1 Uso de variável

`lookup_variable()`:

```c
Symbol *symbol = st_lookup(name);

if (!symbol) {
    semantic_error("a variavel '%s' nao foi declarada", name);
    return NULL;
}
if (symbol->kind == SYM_FUNCTION) {
    semantic_error("'%s' e uma funcao e nao pode ser usada como variavel", name);
    return NULL;
}
```

É usada em:

- expressões com identificador;
- atribuições;
- incremento/decremento;
- inicialização de `for`;
- atualização de `for`.

### 7.2 Chamada de função

`lookup_function()`:

1. busca apenas no escopo global;
2. exige que o símbolo seja uma função;
3. verifica a quantidade de argumentos.

Como a busca é feita antes de o parser continuar, uma função usada antes de
sua declaração é rejeitada — comportamento necessário porque a tradução para
C também exige declaração prévia (caso `E17`).

### 7.3 Verificações de tipo

O parser transporta o tipo estático de cada expressão em `%type <ival> expr`.
Com isso, verifica:

- operandos numéricos em `+`, `-`, `*`, `/`;
- operandos inteiros em `%`;
- operandos numéricos em comparações relacionais;
- compatibilidade em `==` e `!=`;
- operandos `boolean` em `&&`, `||` e `!`;
- condições `boolean` em `if`, `while`, `for` e `do while`;
- compatibilidade em atribuições, inicializações e `return`.

As conversões implícitas aceitas seguem a promoção do subconjunto:

```text
char -> int -> float -> double
```

`boolean` não é convertido implicitamente para tipos numéricos, e vice-versa.

### 7.4 Controle de laços e retorno

O parser mantém dois estados auxiliares:

- `loop_depth`: incrementado ao entrar em `while`, `for` ou `do while` e
  decrementado ao sair. `break` e `continue` só são aceitos com
  `loop_depth > 0`.
- `current_return_type`: tipo de retorno do método atual. Usado para validar
  `return` com e sem valor.

---

## 8. Exemplo passo a passo

Considere o caso `examples/escopo/E09.java`:

```java
static int contador = 0;

static void inc() {
    int contador = 5;
    contador = contador + 1;
    System.out.println(contador);
}
```

Sequência de operações:

1. `st_init()` cria o escopo global (nível 0).
2. O parser entra no escopo `main` implícito (nível 1).
3. `static int contador = 0;` insere `contador` como `SYM_GLOBAL` no nível 0.
4. `static void inc()` insere `inc` como `SYM_FUNCTION` no nível 0 e entra no
   escopo do método (nível 2, `parent = global`).
5. `int contador = 5;` no corpo do método:
   - `st_lookup_local_visible("contador")` não encontra nada (o global não é
     considerado nessa busca);
   - o símbolo é inserido no escopo do bloco (nível 3) como `SYM_VARIABLE`.
6. `contador = contador + 1;` resolve as duas ocorrências para a variável
   local do bloco.
7. Ao sair do bloco, o símbolo local é liberado.
8. Ao sair do método, os parâmetros (nenhum, neste caso) e o escopo do método
   são liberados.
9. Ao fim do programa, `--tabela` poderia mostrar apenas o escopo global com
   `contador` e `inc`. Sem `--tabela`, a tabela é liberada por `st_free()`.

Esse exemplo demonstra as duas regras principais ao mesmo tempo:

- a variável local **pode** ocultar a global;
- a variável local **não** é visível fora do bloco/método.

---

## 9. Visualizando a tabela

O executável aceita `--tabela` (ou `-t`):

```bash
./protoparser --tabela examples/escopo/E08.java
```

A impressão ocorre antes de o parser sair do escopo `main` implícito, então
aparecem:

- o escopo local do `main` implícito, se houver símbolos;
- o escopo global com campos e funções.

Exemplo de saída para um programa com funções e campo global:

```text
=== Tabela de simbolos ===
  NOME             CATEGORIA  TIPO     LINHA      ESCOPO
  -- escopo local nivel 1 --
  (vazio)
  -- escopo global --
  soma             funcao     int      linha 4    escopo 0  (int, int) [inicializado]
  contador         global     int      linha 2    escopo 0 [inicializado]
  main             funcao     void     linha 12   escopo 0  (String) [inicializado]
```

Os escopos de métodos e blocos já foram fechados quando a impressão acontece,
por isso não aparecem nesse dump.

---

## 10. Erros semânticos

Os erros são reportados em `stderr` no formato:

```text
ERRO SEMANTICO na linha N: <descrição>
```

Exemplos reais gerados pelo compilador:

```text
ERRO SEMANTICO na linha 4: a variavel 'x' nao foi declarada
ERRO SEMANTICO na linha 3: a variavel 'x' ja foi declarada neste escopo
ERRO SEMANTICO na linha 5: a funcao 'soma' ja foi declarada (sobrecarga nao e suportada)
ERRO SEMANTICO na linha 2: a funcao 'dobro' nao foi declarada antes do uso
ERRO SEMANTICO na linha 2: a variavel 'i' ja foi declarada neste escopo
```

A flag global `had_semantic_error` é marcada a cada erro. `main()` só considera
a compilação bem-sucedida quando:

```c
parse_result == 0
&& !had_lexical_error
&& !had_semantic_error
```

Assim, nenhum código C deve ser gerado quando houver erro léxico, sintático ou
semântico.

---

## 11. Limitações atuais

- A tabela guarda apenas nomes de variáveis, parâmetros, campos e funções.
  Classes não são inseridas como símbolos, pois no subconjunto a classe é
  apenas uma casca sintática removida na geração de C.
- Arrays não fazem parte do sistema de tipos. A única exceção reconhecida é
  `String[] args` na assinatura do `main`.
- O campo `initialized` é registrado, mas a linguagem ainda não exige que toda
  variável local seja inicializada antes do uso (caso `E11` é aceito).
- A análise é feita durante o parsing. Ainda não existe uma AST separada nem
  uma etapa posterior de geração de código C.
- Como os escopos são liberados ao sair, a tabela não mantém um histórico
  completo de todos os símbolos já declarados. Para depuração, use
  `--tabela`, que imprime os escopos ativos no fim do programa.

---

## 12. Resumo

- Cada escopo tem uma tabela hash própria (211 buckets, encadeamento
  separado).
- A busca percorre a cadeia de escopos do mais interno para o mais externo.
- Inserir no mesmo escopo é erro; ocultar uma variável local visível é erro;
  ocultar uma global é permitido.
- O escopo de método usa `parent = global` e `previous = escopo anterior` para
  isolar métodos do `main` implícito.
- Blocos e `for` abrem e fecham escopos, descartando os símbolos ao sair.
- O parser usa a tabela para verificar declaração, uso, tipos, retorno,
  `break`/`continue` e sobrecarga de métodos.
