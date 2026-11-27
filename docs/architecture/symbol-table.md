# Tabela de símbolos

A tabela de símbolos (`src/symbol_table.{h,c}`) é uma pilha de escopos. Cada
escopo possui uma tabela hash própria (211 buckets, hash djb2 com encadeamento
separado) e aponta para o escopo pai. A busca começa no escopo atual e sobe até
o global.

As assinaturas estão na [API da tabela de símbolos](../reference/symbol-table-api.md);
esta página descreve o modelo e as regras.

## Níveis de escopo

| Nível | Conteúdo |
| --- | --- |
| `0` | escopo global: campos `static` (globais em C) e métodos `static` (funções) |
| `1` | escopo `main` implícito para comandos soltos no topo do arquivo |
| `2+` | escopos de métodos e de blocos/`for` |

`st_enter_function_scope()` usa o escopo global como pai, de modo que parâmetros
e locais de um método **não** enxergam as variáveis locais do `main` implícito.
Ao sair de um bloco ou `for`, seus símbolos são descartados.

## Símbolos

Cada `Symbol` guarda:

- nome (cópia própria), categoria (`SYM_VARIABLE`, `SYM_PARAMETER`,
  `SYM_GLOBAL`, `SYM_FUNCTION`) e tipo (`ST_TYPE_*`);
- linha de declaração e nível de escopo;
- estado de inicialização;
- para funções, quantidade e tipos dos parâmetros (assinatura).

`ST_TYPE_STRING` existe apenas para reconhecer `String[] args` de `main`;
`ST_TYPE_UNKNOWN` evita cascatas de erro após uma expressão já inválida.

## Verificações durante o parsing

As ações em `parser/parser.y` usam a tabela para verificar:

- uso de variável ou função não declarada;
- redeclaração no mesmo escopo;
- ocultação: local pode esconder global, mas não pode esconder outro local
  visível nem parâmetro;
- visibilidade da variável de `for` apenas dentro do `for`;
- declaração da função antes da chamada e proibição de sobrecarga;
- compatibilidade de tipos em atribuições, inicializações, retornos e
  operadores;
- operandos de `&&`, `||` e `!` e condições de `if`, `while`, `for` e
  `do while` do tipo `boolean`;
- `break` e `continue` somente dentro de laços;
- constante `int` que cabe em `char` em declaração/retorno; a mesma conversão
  não é aplicada a argumentos de chamada.

Promoções numéricas implícitas seguem `char → int → float → double`. Tipos
booleanos não são convertidos para numéricos.

## Análise de fluxo posterior

Algumas regras dependem do programa inteiro e são aplicadas por
`src/semantic_flow.c` sobre a AST completa, depois do parsing:

- método com retorno não-`void` precisa retornar em todos os caminhos;
- código inalcançável é erro (comandos após `return`/`break`/`continue`, corpo
  de `while (false)`);
- variável local precisa estar inicializada antes de ser lida, considerando
  todos os caminhos.

`src/const_eval.c` avalia expressões constantes para essas decisões. Como não
há `final`, nenhuma variável é constante; apenas literais e operadores sobre
literais são avaliados. Condições constantes contam como no `javac`: em
`if (true) { x = 5; }`, `x` é considerado atribuído.

## Erros e depuração

Erros semânticos usam o formato:

```text
ERRO SEMANTICO na linha N: <descrição>
```

A opção `--tabela` imprime os escopos ativos ao final da análise:

```text
=== Tabela de simbolos ===
  NOME             CATEGORIA  TIPO     LINHA      ESCOPO
  -- escopo local nivel 1 --
  (vazio)
  -- escopo global --
  inc              funcao     void     linha 3    escopo 0  () [inicializado]
  contador         global     int      linha 1    escopo 0 [inicializado]
```

Os casos de teste em `examples/escopo/` cobrem essas regras; o formato do
manifesto está em [Executando testes](../guides/testing.md).

!!! note "Especificação revisada"
    O arquivo `docs/tabela-simbolos.md` foi atualizado em relação ao código:
    usa os nomes `ST_TYPE_*` do header atual, descreve a AST, a análise de fluxo
    e a geração de C e registra que `E11` é rejeitado. Versões antigas desse
    documento afirmavam o contrário; o conteúdo versionado hoje é a referência
    em conjunto com os testes.
