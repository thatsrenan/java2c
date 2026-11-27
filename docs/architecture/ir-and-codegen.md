# Representação intermediária e geração de C

A representação intermediária (IR) do java2c é uma lista linear de instruções
de **três endereços** (TAC). Ela é produzida a partir da AST validada, pode ser
otimizada e serve de entrada ao gerador de código C.

## Formato da TAC

A saída de `--ir` começa com `=== IR (TAC) ===`. Um exemplo simplificado,
baseado nos arquivos de `examples/ir/`, tem esta forma:

```text
=== IR (TAC) ===
global contador: int = 0

func soma(a: int, b: int) -> int
locals:
temps: t0: int
t0 = a + b
return t0

func main() -> void
locals: x: int
temps:
param 10
param 20
t0 = call soma, 2
x = t0
print x
return
```

Elementos observáveis nos exemplos versionados:

| Elemento | Formato | Exemplo |
| --- | --- | --- |
| Global | `global nome: tipo = valor` | `global metade: double = 0.5` |
| Função | `func nome(param: tipo, ...) -> tipo` | `func soma(a: int, b: int) -> int` |
| Locais e temporários | listas `locals:` e `temps:` com tipo | `locals: y: double, y_1: int` |
| Atribuição | `dest = expressao` | `t1 = i % 2` |
| Desvio condicional | `ifFalse tN goto ROTULO` | usado por `if`, laços e curto-circuito |
| Rótulos | `LnomeN:` | `Lwhile_cond0:`, `Lendif2:`, `Ldo_body7:` |
| Argumento de chamada | `param valor` | `param 20` |
| Chamada | `tN = call nome, quantidade` | `t0 = call soma, 2` |
| Impressão | `print valor` | `print i` |
| Cast | `dest = (tipo) origem` | `t1 = (char) t0` |

Inicializadores globais que dependem de cálculo são movidos para uma função
`__init_globals`, como em `examples/ir/I08.tac`.

## Nomes únicos

Dentro de uma função, cada nome precisa ser distinto. O gerador renomeia:

- locais de blocos diferentes com o mesmo nome (`y`, `y_1`);
- locais que ocultam globais;
- identificadores que colidem com palavras reservadas de C ou com
  temporários `tN`.

Esse comportamento é verificado pelos casos I07, I09 e I11 em
`examples/ir/` e pelo caso C07 em `examples/codegen/`.

## Otimização

`src/ir_opt.c` aplica otimizações sobre a TAC até atingir um ponto fixo. Cada
passo monta um grafo de fluxo de controle, propaga constantes, remove blocos
inalcançáveis, limpa saltos e rótulos, aplica eliminação local de subexpressões
comuns e remove temporários. `tests/ir_opt_test.c` exercita essas rotinas
diretamente e `make test-ir-opt` executa o teste.

`ir_generate()` chama `ir_optimize()` antes de retornar, de modo que a TAC
impressa por `--ir` já é a versão otimizada; a geração de C usa essa mesma IR.

## Geração de C

`src/codegen.c` traduz a IR para C. Os scripts de teste exercitam dois modos:

| Modo | Característica |
| --- | --- |
| `--c` | saída direta, com temporários e desvios explícitos |
| `--c-clean` | expressões em linha, com impressão de reais via `printf("%g")` |

O modo bruto usa `int32_t`, `uint16_t` e `bool` e denomina `__j2c_main` a função
de entrada quando há um `main` na entrada. O modo limpo usa `int` e
`unsigned short` e imprime reais com `%g`, fazendo com que um valor exatamente
inteiro apareça como `2` onde o modo bruto mostra `2.0`; por isso alguns casos
têm `NOME.clean.out`.

O C gerado é compilado nos testes com `-std=c11 -Wall -Wextra -Werror
-pedantic -fwrapv` e executado. As saídas em `examples/codegen/*.out` registram
o comportamento observável: aritmética e casts (C01), laços e condicionais
(C02), curto-circuito (C03), recursão (C04), globais (C05), escapes em strings
(C06), colisões de nomes com C (C07), classe sem `main` (C08), caracteres
(C09) e conversões numéricas (C10).

## Leituras relacionadas

- [Visão geral da arquitetura](overview.md) — onde a IR se encaixa no pipeline.
- [API da IR e geração de C](../reference/ir-api.md) — funções e estruturas.
- [Executando testes](../guides/testing.md) — como as saídas são comparadas.
