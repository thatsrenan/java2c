# java2c

Compilador do subconjunto procedural de Java para C, implementado com Flex
(analisador léxico) e Bison (analisador sintático), com tabela de símbolos e
verificações de escopo/declaração.

## Documentação

A documentação técnica completa está na pasta `docs/` e pode ser executada
localmente com:

```bash
pip install -r requirements-docs.txt
mkdocs serve
```

A versão publicada ficará em <https://thatsrenan.github.io/java2c/> após a
habilitação do GitHub Pages (ver
[documentação do projeto](docs/development/documentation.md)).

As especificações Markdown em `docs/` (`definicao-escopo-sintaxe-semantica.md`,
`definicao-tokens.md` e `tabela-simbolos.md`) foram revisadas em relação ao
código atual. O PDF `especificacao-java-subset.pdf` permanece apenas como
rascunho histórico.

## Declaração de uso de IA

Por se tratar de um tema de difícil entendimento e envolver programação
complexa, a IA foi usada de forma geral durante o projeto, com diferentes
modelos, como apoio à equipe. **Nenhuma decisão foi tomada pela IA**: todas as
entregas foram revisadas pelos mantenedores e receberam correções, inclusive
ajustes que ultrapassaram o escopo original. A declaração completa está em
[`docs/development/ai-usage.md`](docs/development/ai-usage.md).

## Estrutura do Repositório

```
.java2c
│
├── docs                       # documentação MkDocs
│   ├── index.md
│   ├── introduction/
│   ├── getting-started/
│   ├── architecture/
│   ├── guides/
│   ├── reference/
│   ├── development/
│   └── definicao-*.md, tabela-simbolos.md  # especificações revisadas
├── examples
│   ├── ast
│   ├── codegen
│   ├── escopo
│   ├── ir
│   ├── lexico
│   ├── sintaxe
│   └── exemplo1.java
├── lexer
│   └── lexer.l
├── parser
│   └── parser.y
├── src
│   ├── main.c
│   ├── lextest.c
│   ├── symbol_table.{c,h}
│   ├── semantic_flow.{c,h}   # retorno, alcançabilidade, inicialização
│   ├── const_eval.{c,h}      # expressões constantes
│   ├── ir.{c,h}              # estruturas e impressão da TAC
│   ├── ir_gen.{c,h}          # AST -> TAC
│   ├── ir_opt.{c,h}          # otimizações da IR
│   └── codegen.{c,h}         # IR -> C
├── tests
│   ├── check.sh
│   └── check_escopo.sh
├── .github/workflows/documentation.yml
├── mkdocs.yml
├── requirements-docs.txt
├── Makefile
├── .gitignore
└── README.md
```

## Como Usar

### 1. Clonar o Repositório

``` bash
git clone https://github.com/thatsrenan/java2c.git
cd java2c
```

### 2. Instalar Dependências

- É necessário ter _Flex_ e _Bison_ instalados no sistema.
- Em distribuições Linux baseadas em Debian/Ubuntu:

```bash
sudo apt update
sudo apt install flex bison build-essential git -y
```

### 3. Compilar e executar

```bash
make
./protoparser examples/exemplo1.java
```

Para imprimir a tabela de símbolos ao final da análise:

```bash
./protoparser --tabela examples/escopo/E09.java
```

Para imprimir a representação intermediária (código de três endereços):

```bash
./protoparser --ir examples/ir/I03.java
```

O código de saída é `0` apenas quando não há erro léxico, sintático ou
semântico.

## Tabela de Símbolos

A tabela de símbolos fica em `src/symbol_table.{h,c}` e é organizada como uma
pilha de escopos, cada um com sua própria tabela hash:

- **escopo global (nível 0):** campos `static` (variáveis globais em C) e
  métodos `static` (funções em C);
- **escopo `main` implícito (nível 1):** comandos soltos no topo do arquivo,
  tratados como variáveis locais;
- **escopo de método:** parâmetros;
- **escopo de bloco/for:** variáveis locais, descartadas ao sair do bloco.

Cada símbolo guarda nome, categoria (variável, parâmetro, global, função),
tipo, linha de declaração, nível de escopo, estado de inicialização e, para
funções, a assinatura de parâmetros.

As verificações realizadas incluem:

- uso de variável/função não declarada;
- redeclaração no mesmo escopo e ocultação de variável local visível;
- variável de `for` visível apenas dentro do `for`;
- variável local pode ocultar uma global;
- função deve ser declarada antes do uso;
- sobrecarga de métodos não é suportada;
- tipos incompatíveis em atribuições, inicializações, retornos e operadores;
- condições de `if`, `while`, `for` e `do while` devem ser `boolean`;
- `break`/`continue` somente dentro de laços;
- `return` compatível com o tipo de retorno do método;
- constante `int` que cabe em `char` pode ser atribuída a `char`
  (`char c = 65;`), como em Java; em argumentos de chamada, não.

Depois do parser, uma análise de fluxo sobre a AST (`src/semantic_flow.c`,
regras do Java) verifica ainda:

- método não-`void` retorna um valor em todos os caminhos;
- código inalcançável (depois de `return`/`break`/`continue`, corpo de
  `while (false)`) é erro;
- variável local precisa estar inicializada antes de ser lida, considerando
  todos os caminhos (`if (true) { x = 5; }` conta como atribuição, como no
  javac).

Erros semânticos são reportados no formato:

```text
ERRO SEMANTICO na linha N: <descrição>
```

## Representação Intermediária

`./protoparser --ir` gera código de três endereços (TAC) a partir da AST
validada. Cada função lista parâmetros, locais (`locals:`) e temporários
(`temps:`) com tipo; toda global tem valor inicial (o padrão do tipo, se o
Java não der um). Nomes de variáveis são únicos dentro de cada função:
locais de blocos diferentes com o mesmo nome, ou que ocultam uma global,
são renomeadas (`y`, `y_1`), assim como nomes que colidem com palavras
reservadas de C ou com temporários (`tN`).

## Geração de C

A IR otimizada também é usada para gerar C:

```bash
./protoparser --c -o saida.c examples/codegen/C01.java       # com temporários
./protoparser --c-clean -o saida.c examples/codegen/C01.java # expressões em linha
```

## Testes

```bash
make check               # suíte completa de conferências
make test-check          # lexer contra examples/lexico/esperado.txt
make test-escopo-check   # parser + tabela de símbolos contra examples/escopo/esperado.txt
make test-ir-check       # IR (TAC) contra examples/ir/*.tac
make test-codegen-check  # C gerado compilado e executado contra examples/codegen
make test-lexico         # mostra entrada/saída de cada caso léxico
make test-escopo         # mostra entrada/saída de cada caso de escopo
```

## Como Contribuir

1. Faça um _fork_ do repositório.
2. Crie uma _branch_ para suas alterações:

``` bash
git checkout -b minha-feature
```

3. Faça _commit_ e envie um _pull request_.
