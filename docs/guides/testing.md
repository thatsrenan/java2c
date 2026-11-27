# Executando testes

A suíte é composta por binários de teste e por scripts que comparam a saída do
compilador com arquivos de expectativa em `examples/`. Todos os alvos estão
definidos no [Makefile](https://github.com/thatsrenan/java2c/blob/main/Makefile).

## Visão geral

```bash
make check
```

`make check` executa, em ordem, as conferências de léxico, AST unitária,
otimizador de IR, gerador de C, sintaxe, AST do parser, memória, escopo,
IR e geração de código ponta a ponta. É o comando usado antes de abrir um
pull request.

| Alvo | Entrada | Critério |
| --- | --- | --- |
| `test-check` | `examples/lexico/*.java` x `esperado.txt` | sequência exata de tokens, status de saída e número de linhas `ERRO LEXICO` |
| `test-sintaxe` | `examples/sintaxe/*.java` x `esperado.txt` | programa aceito (saída `0`) ou rejeitado com o texto esperado |
| `test-ast-parser` | `examples/ast/*.java` x `*.ast` | saída de `--ast` idêntica ao arquivo `.ast` |
| `test-ast` | `tests/ast_test.c` | construtores, `append`, impressão e liberação da AST |
| `test-mem` | todos os exemplos | execução de `protoparser-san` com AddressSanitizer/UBSan |
| `test-escopo-check` | `examples/escopo/*.java` x `esperado.txt` | caso `ok` aceito; caso `erro` rejeitado com `ERRO SEMANTICO` (não com erro de sintaxe) |
| `test-ir-check` | `examples/ir/*.java` x `*.tac` | saída de `--ir` idêntica ao arquivo `.tac` |
| `test-ir-opt` | `tests/ir_opt_test.c` | otimizações de fluxo da IR |
| `test-codegen` | `tests/codegen_test.c` | geração de C a partir de IR montada manualmente |
| `test-codegen-check` | `examples/codegen/*.java` x `*.out` | C gerado é compilado, executado e comparado com a saída esperada |

Alvos de inspeção manual: `test-lexico` e `test-escopo` mostram entrada e saída
de cada caso. `test-all-save` grava relatórios em `build/resultados/`. O alvo
`test-tokens` do Makefile referencia `examples/tokens/`, que não existe no
repositório; para verificar tokens use `test-check`. A lista completa e os
binários envolvidos estão em
[Ambiente de desenvolvimento](../development/development-setup.md).

## Formatos dos arquivos de expectativa

### Léxico — `examples/lexico/esperado.txt`

```text
L01 ok : WHILE
L32 ok : IDENTIFIER INCREMENT PLUS IDENTIFIER SEMICOLON
L06 erro 1 : ...
```

`NOME ok` exige sucesso e a sequência exata de tokens; `NOME erro N` exige
`N` linhas `ERRO LEXICO` e os tokens que restam. O script `tests/check.sh`
usa `lextest`, que não depende do parser. Fragmentos sintaticamente inválidos
podem ser lexicalmente corretos: nesses casos, o que importa é a ausência de
`ERRO LEXICO`, como explica `examples/lexico/LEIA-ME.md`.

!!! note "Casos especiais"
    Comentários de bloco não aninham; `i+++j` é tokenizado como `i ++ + j`
    (*maximal munch*); `3.14.15` gera `REAL_LITERAL DOT INTEGER_LITERAL` de
    acordo com o manifesto atual (a especificação Markdown sugeria erro).

### Sintaxe — `examples/sintaxe/esperado.txt`

```text
S01 ok
S10 erro : o tipo String nao e suportado
```

Para casos `erro`, o texto após `:` precisa aparecer em stdout ou stderr.
Apenas a gramática é avaliada; não é necessário que o programa passe pela
análise semântica. Exemplos notáveis: `S03` aceita `main` sem `String[] args`,
e `S22` exige bloco após `if`.

### Escopo e semântica — `examples/escopo/esperado.txt`

```text
E09 ok
E11 erro : ERRO SEMANTICO
```

Um caso `erro` só passa se o processo terminar com falha, contiver uma linha
iniciada por `ERRO SEMANTICO` e **não** contiver `ERRO DE SINTAXE`. Isso evita
que um caso seja aprovado por uma rejeição acidental na gramática.

### AST e IR

`tests/check_ast.sh` exige que `protoparser --ast NOME.java` seja idêntico a
`examples/ast/NOME.ast`, incluindo as anotações de linha (`@linha`).
`tests/check_ir.sh` faz o mesmo entre `--ir` e `examples/ir/NOME.tac`.
Qualquer diferença de espaço ou rótulo falha o teste.

### Geração de C — `examples/codegen/`

`tests/check_codegen.sh` roda dois modos:

- `--c`: saída direta da IR, com temporários e desvios explícitos;
- `--c-clean`: versão com expressões em linha; por usar `printf("%g")` para
  reais, valores exatamente inteiros aparecem sem `.0` (`2.0` no modo bruto
  vira `2` no modo limpo).

O C gerado é compilado com `-std=c11 -Wall -Wextra -Werror -pedantic
-fwrapv` e executado; a saída é comparada com `NOME.out` (ou
`NOME.clean.out`). `-fwrapv` reproduz o comportamento de estouro de `int` em
Java.

## Adicionar um caso de teste

1. Crie o arquivo `.java` na pasta do conjunto correspondente.
2. Adicione a linha de expectativa no `esperado.txt` da pasta, ou crie o
   arquivo `.ast`/`.tac`/`.out` com a saída desejada.
3. Rode o alvo específico e depois `make check`.
4. Casos sem expectativa são reportados como `[AVISO]` pelos scripts; isso
   não significa aprovação. Por exemplo, `T22` em `examples/escopo/esperado.txt`
   está marcado como pendente.

!!! tip "Terminação de linha"
    `.gitattributes` fixa LF para `*.sh`, `Makefile` e `examples/*/esperado.txt`.
    Ao editar esses arquivos no Windows, mantenha LF para evitar diffs
    espúrios e falhas de comparação exata.
