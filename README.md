# java2c

Compilador do subconjunto procedural de Java para C, implementado com Flex
(analisador léxico) e Bison (analisador sintático), com tabela de símbolos e
verificações de escopo/declaração.

## Estrutura do Repositório

```
.java2c
│
├── docs
│   ├── definicao-escopo-sintaxe-semantica.md
│   ├── definicao-tokens.md
│   └── tabela-simbolos.md
├── examples
│   ├── escopo
│   ├── lexico
│   └── exemplo1.java
├── lexer
│   └── lexer.l
├── parser
│   └── parser.y
├── src
│   ├── main.c
│   ├── lextest.c
│   ├── symbol_table.c
│   └── symbol_table.h
├── tests
│   ├── check.sh
│   └── check_escopo.sh
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
- `return` compatível com o tipo de retorno do método.

Erros semânticos são reportados no formato:

```text
ERRO SEMANTICO na linha N: <descrição>
```

## Testes

```bash
make test-check          # lexer contra examples/lexico/esperado.txt
make test-escopo-check   # parser + tabela de símbolos contra examples/escopo/esperado.txt
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
