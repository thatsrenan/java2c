# Usando o compilador

Os exemplos abaixo usam arquivos versionados em `examples/`. As opções e seus
contratos estão na [referência da CLI](../reference/cli.md); aqui o foco é o
fluxo de trabalho.

## Analisar um arquivo

```bash
./protoparser examples/exemplo1.java
echo $?
```

O modo padrão mostra a AST e a mensagem de conclusão. `$?` é `0` quando não há
erro léxico, sintático ou semântico e `1` caso contrário. Mensagens de erro
seguem os prefixos `ERRO LEXICO`, `ERRO DE SINTAXE` e `ERRO SEMANTICO`.

Quando nenhum arquivo é informado, o compilador lê da entrada padrão:

```bash
cat examples/exemplo1.java | ./protoparser
```

## Escolher a saída

=== "Tabela de símbolos"

    ```bash
    ./protoparser --tabela examples/escopo/E09.java
    ```

    `--tabela` complementa o modo padrão; para ver somente a AST use `--ast`.

=== "AST"

    ```bash
    ./protoparser --ast examples/ast/A01.java > arvore.txt
    ```

    Cada nó recebe `@linha`, permitindo comparar com `examples/ast/A01.ast`.

=== "Representação intermediária"

    ```bash
    ./protoparser --ir examples/ir/I03.java
    ```

    A saída começa com `=== IR (TAC) ===` e já contém as otimizações aplicadas
    por `ir_generate()`.

=== "Código C"

    ```bash
    ./protoparser --c -o bruto.c examples/codegen/C01.java
    ./protoparser --c-clean -o limpo.c examples/codegen/C01.java
    ```

    O modo `--c` mantém temporários e desvios; `--c-clean` produz C mais
    legível. `--c-limpo` é sinônimo de `--c-clean`. `-o` só é aceito com um
    desses dois modos.

## Compilar e executar o C gerado

```bash
./protoparser --c-clean -o /tmp/C01.c examples/codegen/C01.java
gcc -std=c11 -Wall -Wextra -Werror -pedantic -fwrapv -o /tmp/C01 /tmp/C01.c
/tmp/C01
```

A saída esperada, registrada em `examples/codegen/C01.out`, é:

```text
11
5
2.5
3
```

`-fwrapv` reproduz o comportamento de estouro de inteiros de Java, conforme
usado nos scripts de teste.

## Explorar os exemplos

| Pasta | O que contém | Como verificar |
| --- | --- | --- |
| `examples/lexico/` | casos L01-L56 e `esperado.txt` | `make test-check` |
| `examples/sintaxe/` | casos S01-S24 e `esperado.txt` | `make test-sintaxe` |
| `examples/ast/` | casos A01-A05 e árvores `.ast` | `make test-ast-parser` |
| `examples/escopo/` | casos E/F/G/T e `esperado.txt` | `make test-escopo-check` |
| `examples/ir/` | casos I01-I11 e arquivos `.tac` | `make test-ir-check` |
| `examples/codegen/` | casos C01-C10 e saídas `.out` | `make test-codegen-check` |

Para inspecionar entrada e saída de todos os casos de uma pasta:

```bash
make test-lexico
make test-escopo
```

## Limitações no uso

- Não há arquivo de configuração: o comportamento é escolhido na linha de
  comando. Veja [Configuração](../getting-started/configuration.md).
- `String` não é um tipo da linguagem e arrays são rejeitados, exceto o
  parâmetro convencional `String[] args` de `main`.
- Sobrecarga de métodos e chamada antes da declaração são erros.
- Um erro em qualquer etapa impede a geração de IR e de C.
- O alvo `test-tokens` do Makefile aponta para `examples/tokens/`, que não
  existe no repositório; use `make test-check` para conferir tokens.
