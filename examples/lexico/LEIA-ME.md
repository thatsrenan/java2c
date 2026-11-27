# Como ler os testes léxicos

## Visão geral

Esta pasta contém **56 casos** (`L01.java` a `L56.java`) e dois arquivos de
referência com papéis diferentes:

| Arquivo | Papel |
| --- | --- |
| `esperado.txt` | Manifesto **atual**, usado por `tests/check.sh` e por `make test-check` |
| `RESULTADOS.txt` | Registro histórico da saída do `protoparser` para os 43 primeiros casos |

O manifesto é a referência automática. `RESULTADOS.txt` não é usado pelos
testes: ele mostra como os casos se comportavam quando o projeto expunha apenas
o compilador completo, antes da criação do harness isolado do lexer.

## Executando os testes

```bash
make
make test-check
```

`make test-check` roda `tests/check.sh ./lextest examples/lexico`.
`lextest` é um harness que **não liga o parser**: ele chama `yylex()` em
laço e imprime um token por linha:

```text
linha  TOKEN             lexema
```

O formato de `esperado.txt` é:

```text
NOME  ok      : TOKEN TOKEN ...
NOME  erro N  : TOKEN TOKEN ...
```

Depois de `:` vem a sequência exata de tokens que o lexer deve devolver; os
tokens descartados por erro não aparecem. Um caso passa apenas se status,
quantidade de erros e sequência de tokens forem iguais ao esperado.

Para inspecionar a saída de todos os casos:

```bash
make test-lexico
```

## Léxico versus sintático

`RESULTADOS.txt` registra a saída do `protoparser`, que usa Flex **e** Bison.
Por isso é comum ver `ERRO DE SINTAXE` em casos que testam apenas um fragmento:

- Casos como `L01.java` (só `while`), `L02.java` (só `whileLoop`) e `L03.java`
  (só `int`) são **lexicamente corretos**: o token foi reconhecido, mas a
  gramática espera uma instrução completa. Nesses casos, o que importa é a
  ausência de `ERRO LEXICO`.
- Casos como `L06.java` (`2x`), `L17.java`–`L19.java` (char malformado),
  `L21.java` (string não terminada), `L26.java` (comentário sem fechar),
  `L44.java`–`L47.java` (reais malformados ou fora do intervalo), `L55.java`
  (inteiro fora do intervalo) e `L56.java` (real `float` fora do intervalo)
  devem apresentar `ERRO LEXICO na linha N: ...`.
- Casos completos e válidos (por exemplo `L07`, `L08`, `L14`, `L20`, `L23`,
  `L24`, `L25`, `L28`–`L31`, `L39`–`L43`, `L50`, `L51`, `L53`) terminam sem
  erros léxicos.

Com `lextest`, essa distinção desaparece: a saída contém apenas os tokens e as
mensagens de erro do lexer, sem o parser.

## Quebras recentes cobertas pelos casos

Os casos `L44` a `L56` cobrem decisões que antes apareciam como pendentes na
documentação e hoje estão implementadas em `lexer/lexer.l`:

- reais malformados (`1.5e`, `1.5abc`) e estouro de `double`/`float`;
- inteiro maior que `INT_MAX`;
- escapes válidos e inválidos em strings;
- BOM UTF-8 e bytes não-ASCII fora de literais/comentários.

O significado de cada linha de `esperado.txt` está documentado no cabeçalho do
próprio arquivo e em [Executando testes](../../docs/guides/testing.md).
