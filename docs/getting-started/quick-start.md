# Início rápido

Este guia percorre os comandos básicos usando apenas exemplos versionados no
repositório. Consulte [Instalação](installation.md) caso ainda não tenha
compilado o projeto.

## 1. Compilar

```bash
make
make check
```

`make` gera `protoparser` e `lextest`. `make check` confirma que a suíte de
testes passa antes de qualquer alteração.

## 2. Analisar um programa

```bash
./protoparser examples/exemplo1.java
```

O modo padrão imprime o banner de início, a AST e a mensagem
`=== Analise concluida sem erros ===`. O código de saída é `0` apenas quando
não há erro léxico, sintático ou semântico.

## 3. Inspecionar cada etapa

=== "Tabela de símbolos"

    ```bash
    ./protoparser --tabela examples/escopo/E09.java
    ```

    A saída inclui os escopos ativos ao final do parsing:

    ```text
    === Tabela de simbolos ===
      NOME             CATEGORIA  TIPO     LINHA      ESCOPO
      -- escopo local nivel 1 --
      (vazio)
      -- escopo global --
      inc              funcao     void     linha 3    escopo 0  () [inicializado]
      contador         global     int      linha 1    escopo 0 [inicializado]
    ```

    `--tabela` é aditivo: as mensagens e a AST do modo padrão continuam
    aparecendo.

=== "AST"

    ```bash
    ./protoparser --ast examples/ast/A01.java
    ```

    Cada nó é anotado com `@linha`:

    ```text
    CLASS (sem classe)  @1
      BLOCK  @1
        VARDECL int a  @1
          BINOP +  @1
            INT 1  @1
            BINOP *  @1
              INT 2  @1
              INT 3  @1
    ```

=== "IR (TAC)"

    ```bash
    ./protoparser --ir examples/ir/I03.java
    ```

    ```text
    === IR (TAC) ===
    global contador: int = 0

    func soma(a: int, b: int) -> int
        temps: t0: int
        t0 = a + b
        return t0
    ```

=== "Código C"

    ```bash
    ./protoparser --c-clean -o /tmp/exemplo.c examples/codegen/C01.java
    gcc -std=c11 -Wall -Wextra -pedantic -fwrapv -o /tmp/exemplo /tmp/exemplo.c
    /tmp/exemplo
    ```

    A saída esperada do programa de exemplo é:

    ```text
    11
    5
    2.5
    3
    ```

## 4. Observar um erro semântico

```bash
./protoparser examples/escopo/E11.java
```

A variável `x` é lida sem valor atribuído, e o compilador reporta:

```text
ERRO SEMANTICO na linha 2: a variavel 'x' pode nao ter sido inicializada
=== Prototipo Flex + Bison: iniciando analise ===
=== Analise concluida com erros ===
```

O processo termina com código `1` e nenhuma IR ou código C é gerado.

## Próximos passos

- [Uso do compilador](../guides/usage.md) — todas as opções e combinações.
- [Linha de comando](../reference/cli.md) — contrato completo de saídas e
  códigos de retorno.
- [Executando testes](../guides/testing.md) — formatos dos manifests e alvos
  disponíveis.
