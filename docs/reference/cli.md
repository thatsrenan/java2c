# Linha de comando

O executável principal é `protoparser`, gerado por `make`. Ele aceita um
arquivo Java e opções que selecionam o que será impresso ao final da análise.

```text
./protoparser [--tabela|--ast|--ir|--c|--c-clean [-o arquivo.c]] [--ajuda] [arquivo.java]
```

## Opções

| Opção | Efeito |
| --- | --- |
| `--tabela`, `-t` | Imprime a tabela de símbolos ao final da análise; pode ser combinada com o modo padrão |
| `--ast` | Imprime somente a AST, com anotações `@linha` |
| `--ir` | Imprime somente a representação intermediária (TAC) |
| `--c` | Gera código C direto da IR, com temporários e desvios explícitos |
| `--c-clean`, `--c-limpo` | Gera C a partir da mesma IR, com expressões em linha e menos ruído |
| `-o arquivo.c` | Destino do código C; válido apenas com `--c` ou `--c-clean` |
| `--ajuda`, `-h` | Imprime o uso e termina com código `0` |

Sem nenhum arquivo, o lexer lê da entrada padrão. Um segundo argumento que não
seja opção produz `argumento inesperado`. `-o` sem caminho produz
`-o requer um caminho de saida`, e `-o` sem `--c`/`--c-clean` produz
`-o so pode ser usado com --c ou --c-clean`; nesses casos a saída é `1`.

## Modos de saída

### Padrão

Sem `--ast`, `--ir` ou `--c`, a saída inclui:

```text
=== Prototipo Flex + Bison: iniciando analise ===
>> Programa reconhecido com sucesso.

--- AST ---
...
=== Analise concluida sem erros ===
```

Se houver erro, a última linha é `=== Analise concluida com erros ===`.
`--tabela` acrescenta o dump dos escopos ativos ao final do parsing.

### `--ast`

```bash
./protoparser --ast examples/ast/A01.java
```

Imprime apenas a árvore, sem as mensagens do modo padrão. É exatamente o que
`tests/check_ast.sh` compara com os arquivos `.ast`.

### `--ir`

```bash
./protoparser --ir examples/ir/I03.java
```

Imprime a TAC depois da otimização. O formato e as garantias estão em
[IR e geração de código](../architecture/ir-and-codegen.md).

### `--c` e `--c-clean`

```bash
./protoparser --c -o /tmp/exemplo.c examples/codegen/C01.java
./protoparser --c-clean examples/codegen/C01.java
```

Sem `-o`, o código C vai para a saída padrão. Os arquivos gerados podem ser
compilados com GCC; os scripts de teste usam
`-std=c11 -Wall -Wextra -Werror -pedantic -fwrapv`.

## Erros e código de saída

O processo termina com `0` somente se:

- o parser não reportou erro sintático;
- o lexer não reportou erro léxico;
- a análise semântica não reportou erro;
- a geração de C, quando solicitada, terminou sem falha.

Qualquer outra situação retorna `1`. Erros de abertura/criação de arquivo
também produzem mensagem em stderr e código `1`:

```text
Nao foi possivel abrir o arquivo: <arquivo>
Nao foi possivel criar o arquivo: <arquivo>
```

As mensagens das etapas de análise usam os prefixos exigidos pelos scripts:

```text
ERRO LEXICO na linha N: <descrição>
ERRO DE SINTAXE na linha N: <descrição>
ERRO SEMANTICO na linha N: <descrição>
```

Um erro impede a geração da IR e do código C.

## Exemplos do repositório

```bash
./protoparser examples/exemplo1.java
./protoparser --tabela examples/escopo/E09.java
./protoparser --ir examples/ir/I03.java
./protoparser --ast examples/ast/A01.java
./protoparser --c-clean examples/codegen/C01.java
```

Mais casos práticos estão em [Uso do compilador](../guides/usage.md).
