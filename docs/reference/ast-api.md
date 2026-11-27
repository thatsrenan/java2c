# API da AST

A árvore sintática abstrata é definida em `parser/ast.h` e implementada em
`parser/ast.c`. As ações da gramática Bison constroem os nós durante o parsing;
as etapas seguintes apenas os percorrem.

## Tipos e categorias

`Type` representa o tipo de uma expressão ou declaração:

```c
typedef enum {
    TYPE_UNKNOWN, TYPE_INT, TYPE_FLOAT, TYPE_DOUBLE, TYPE_BOOLEAN,
    TYPE_CHAR, TYPE_VOID, TYPE_STRING, TYPE_ERROR
} Type;
```

- `TYPE_UNKNOWN` é o valor inicial de todo nó (os nós são criados com
  `calloc`); a análise semântica preenche `type`.
- `TYPE_STRING` descreve apenas o literal de `System.out.println`; não é tipo
  de variável.
- `TYPE_VOID` é usado apenas como tipo de retorno.

`OpKind` cobre operadores aritméticos (`OP_ADD` a `OP_MOD`), relacionais
(`OP_LT` a `OP_NE`), lógicos (`OP_AND`, `OP_OR`, `OP_NOT`, `OP_NEG`), de
atribuição (`OP_ASSIGN`, `OP_ADD_ASSIGN`, ...), incremento/decremento
(`OP_PREINC`, `OP_POSTINC`, `OP_PREDEC`, `OP_POSTDEC`).

`NodeKind` identifica expressões (`NODE_INT`, `NODE_FLOAT`, `NODE_CHAR`,
`NODE_STR`, `NODE_BOOL`, `NODE_IDENTIFIER`, `NODE_BINOP`, `NODE_UNARY`,
`NODE_CALL`, `NODE_CAST`), comandos (`NODE_ASSIGN`, `NODE_VARDECL`,
`NODE_INCDEC`, `NODE_PRINT`, `NODE_IF`, `NODE_WHILE`, `NODE_FOR`,
`NODE_DOWHILE`, `NODE_BREAK`, `NODE_CONTINUE`, `NODE_RETURN`, `NODE_BLOCK`)
e estrutura do programa (`NODE_PARAM`, `NODE_METHOD`, `NODE_CLASS`).

## Estrutura `Node`

| Campo | Uso |
| --- | --- |
| `kind`, `line` | tipo do nó e linha no arquivo de entrada |
| `type`, `decl_type` | tipo inferido da expressão e tipo declarado/destino do cast |
| `op` | operador, quando aplicável |
| `is_global` | `1` em `NODE_VARDECL` de campo `static` |
| `name` | identificador, literal em texto cru ou nome de classe/método |
| `val.i`, `val.d` | valor de literais inteiros/booleanos e reais |
| `a`, `b`, `c`, `d` | filhos estruturais (condição, corpo, bloco `else`, etc.) |
| `next` | próximo irmão em listas de comandos, parâmetros e argumentos |

`next` nunca é um filho estrutural. Um nó tem um único dono: não reutilize o
mesmo nó em duas listas. As strings passadas aos construtores com parâmetro
`char *` são transferidas para o nó; elas não fazem `strdup`.

## Construção

| Função | Retorno / efeito |
| --- | --- |
| `Node *new_node(NodeKind kind, int line)` | nó genérico, com campos zerados |
| `Node *append(Node *list, Node *n)` | anexa `n` ao fim da lista ligada por `next` |
| `Node *new_int(long v, int line)` | literal inteiro |
| `Node *new_real(double v, int line)` | literal real; `decl_type` distingue `float` de `double` |
| `Node *new_char(char *text, int line)` | literal `char` a partir do texto cru |
| `Node *new_str(char *text, int line)` | literal de string (somente em `NODE_PRINT`) |
| `Node *new_bool(int v, int line)` | literal booleano (`0`/`1`) |
| `Node *new_ident(char *name, int line)` | referência a identificador |
| `Node *new_binop(OpKind op, Node *l, Node *r, int line)` | expressão binária |
| `Node *new_unary(OpKind op, Node *operand, int line)` | `OP_NOT` ou `OP_NEG` |
| `Node *new_call(char *name, Node *args, int line)` | chamada; `args` é uma lista de expressões ou `NULL` |
| `Node *new_cast(Type target, Node *operand, int line)` | cast explícito |
| `Node *new_assign(OpKind op, char *name, Node *expr, int line)` | atribuição simples ou composta |
| `Node *new_vardecl(Type t, char *name, Node *init, int line)` | variável local |
| `Node *new_field(Type t, char *name, Node *init, int line)` | campo `static` (variável global) |
| `Node *new_incdec(OpKind op, char *name, int line)` | `++`/`--` |
| `Node *new_param(Type t, char *name, int line)` | parâmetro de método |
| `Node *new_method(Type ret, char *name, Node *params, Node *body, int line)` | método; `body` é um `NODE_BLOCK` |
| `Node *new_class(char *name, Node *members, int line)` | classe; `name` é `NULL` em programa sem classe |

As convenções de preenchimento de `a`, `b`, `c` e `d` por tipo de nó estão
documentadas no próprio `parser/ast.h`. Por exemplo, em `NODE_IF`, `a` é a
condição, `b` o bloco `then` e `c` o bloco `else`, outro `NODE_IF` ou `NULL`.

## Utilitários

| Função / variável | Uso |
| --- | --- |
| `void print_ast(Node *n, int indent)` | imprime a árvore; cada nó recebe `@linha` quando `ast_print_lines != 0` |
| `void free_ast(Node *n)` | libera a árvore; usado também quando o parsing termina com erro |
| `const char *type_name(Type t)` | nome legível do tipo |
| `const char *op_name(OpKind op)` | nome legível do operador |
| `Node *ast_root` | raiz da AST construída pelo parser |
| `int ast_print_lines` | controla a anotação `@linha` na impressão |

Exemplo mínimo de construção:

```c
Node *soma = new_binop(OP_ADD, new_int(1, 7), new_int(2, 7), 7);
Node *decl = new_vardecl(TYPE_INT, strdup("x"), soma, 7);
```

A opção `--ast` do executável chama `print_ast(ast_root, 0)` com
`ast_print_lines = 1`; os arquivos em `examples/ast/` são a saída exata
desse modo.
