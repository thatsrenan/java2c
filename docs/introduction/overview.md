# Visão geral

## Contexto

**java2c** traduz um **subconjunto predominantemente procedural de Java** para
C. Na linguagem aceita, uma classe funciona como casca sintática e seus
métodos `static` correspondem a funções em C; campos `static` correspondem a
variáveis globais. Essa definição aparece em
`docs/definicao-escopo-sintaxe-semantica.md` e é seguida pela gramática em
`parser/parser.y`.

O repositório contém um protótipo completo em C, construído com Flex
(analisador léxico) e Bison (analisador sintático). Além da tradução, o
projeto registra especificações de escopo, tokens e tabela de símbolos, além de
uma suíte de testes baseada em exemplos.

## Objetivos

O pipeline implementado cobre as seguintes etapas:

1. Reconhecer os tokens do subconjunto e acusar erros léxicos.
2. Analisar a gramática e construir uma árvore sintática abstrata (AST).
3. Verificar escopo, declaração, tipos e uso de variáveis com uma tabela de
   símbolos.
4. Aplicar análise de fluxo: retorno em todos os caminhos, alcançabilidade e
   inicialização de variáveis locais.
5. Gerar código de três endereços (TAC) a partir da AST validada e aplicar
   otimizações de fluxo.
6. Gerar código C a partir da IR, em dois modos (com temporários explícitos ou
   com expressões simplificadas).

Não se gera código C quando há erro léxico, sintático ou semântico.

## Escopo da linguagem

A especificação Markdown define como suportados:

- tipos primitivos `int`, `float`, `double`, `boolean`, `char` e `void` (este
  último apenas como tipo de retorno);
- variáveis locais com tipo explícito, campos `static` e métodos `static`;
- operadores aritméticos, relacionais, lógicos, de atribuição, `++`/`--` e
  casts explícitos;
- `if`/`else`, `while`, `for`, `do while`, `break` e `continue`, com blocos
  obrigatórios no `if`;
- `return` e a forma simplificada de impressão `System.out.println(...)`;
- comentários de linha e de bloco.

`String` não é um tipo da linguagem. A única exceção registrada é `String[]
args` no parâmetro de `main`. Arrays, herança, interfaces, generics,
exceções, sobrecarga de métodos e bibliotecas externas estão fora do escopo.

A implementação também aceita algumas formas que a especificação não destaca:
programas sem classe (comandos no topo do arquivo), classe sem `public`,
`main` sem `String[] args` e programa sem método `main`. Elas aparecem nos
exemplos e são verificadas pelos testes; consulte
[Executando testes](../guides/testing.md).

## Limitações conhecidas

- **Sobrecarga não é suportada.** A tradução para C exige um nome distinto por
  função; programas Java com sobrecarga são rejeitados.
- **Declaração antes do uso.** Funções precisam ser declaradas antes da
  chamada, como exigido pelo código C gerado.
- **Erros semânticos bloqueiam a geração.** Um erro léxico, sintático ou
  semântico encerra o processo com código de saída diferente de zero.
- **Rascunho histórico em PDF.** O arquivo
  `especificacao-java-subset.pdf` é um rascunho anterior: ele trata
  `do while` como não suportado, deixa arrays e sobrecarga como decisões
  pendentes e inclui `long` na cadeia de conversão. Ele permanece no
  repositório apenas como registro histórico.
- **Especificações Markdown revisadas.** Os arquivos
  `definicao-escopo-sintaxe-semantica.md`, `definicao-tokens.md` e
  `tabela-simbolos.md` foram atualizados para descrever o código atual: o
  lexer implementa escapes, notação científica e sufixos de literais; a tabela
  de símbolos convive com a AST, a análise de fluxo e a geração de C. As
  páginas desta seção descrevem o mesmo comportamento, com links para as
  especificações na seção **Referência**.

## Continue a leitura

- [Instalação](../getting-started/installation.md) — dependências e compilação.
- [Início rápido](../getting-started/quick-start.md) — primeiros comandos.
- [Arquitetura](../architecture/overview.md) — componentes e fluxo de dados.
- [Uso do compilador](../guides/usage.md) — exemplos práticos.
- [Referência da linha de comando](../reference/cli.md) — opções e saídas.
- [Especificações da linguagem](../definicao-escopo-sintaxe-semantica.md) —
  escopo/sintaxe, tokens e tabela de símbolos revisados em relação ao código.
