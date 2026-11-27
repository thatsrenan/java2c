# java2c

**java2c** é um compilador para um **subconjunto predominantemente procedural
de Java** que gera código C. O front-end usa Flex para análise léxica e Bison
para análise sintática; as etapas seguintes são escritas em C.

A ideia central é aproveitar a semelhança entre métodos `static` de Java e
funções em C: uma classe é tratada como casca sintática, campos `static`
tornam-se variáveis globais e métodos `static` tornam-se funções.

!!! note "Escopo da documentação"
    Esta documentação descreve **o que está implementado no repositório**:
    análise léxica e sintática, AST, tabela de símbolos, análise de fluxo,
    representação intermediária (TAC), otimizações e geração de C. As
    especificações de [escopo e sintaxe](definicao-escopo-sintaxe-semantica.md),
    [tokens](definicao-tokens.md) e
    [tabela de símbolos](tabela-simbolos.md) foram revisadas em relação ao
    código e estão na seção **Referência**. O PDF
    `especificacao-java-subset.pdf` permanece apenas como rascunho histórico,
    conforme a [visão geral](introduction/overview.md).

## O que o compilador faz

- Reconhece os tokens e a gramática do subconjunto, com mensagens de erro
  léxico, sintático e semântico.
- Constrói uma AST e verifica escopo, declaração, tipos, retorno em todos os
  caminhos, alcance e inicialização de variáveis locais.
- Gera código de três endereços com nomes únicos por função e aplica
  otimizações antes da saída.
- Gera código C em dois modos: saída direta da IR ou versão com expressões
  simplificadas.
- Oferece uma suíte de testes baseada em exemplos versionados, executável com
  `make check`.

## Tecnologias

| Camada | Ferramenta |
| --- | --- |
| Analisador léxico | Flex |
| Analisador sintático | Bison |
| Compilador | C (GCC) |
| Build e testes | GNU Make, scripts Bash |
| Documentação | MkDocs, Material for MkDocs |
| Publicação | GitHub Pages, GitHub Actions |

## Comece por aqui

=== "Uso"

    - [Instalação](getting-started/installation.md)
    - [Configuração](getting-started/configuration.md)
    - [Início rápido](getting-started/quick-start.md)
    - [Usando o compilador](guides/usage.md)

=== "Arquitetura"

    - [Visão geral](architecture/overview.md)
    - [Estrutura do repositório](architecture/structure.md)
    - [Fluxo de compilação](architecture/pipeline.md)
    - [IR e geração de C](architecture/ir-and-codegen.md)

=== "Referência"

    - [Linha de comando](reference/cli.md)
    - [API da AST](reference/ast-api.md)
    - [API da tabela de símbolos](reference/symbol-table-api.md)
    - [API da IR e geração de C](reference/ir-api.md)

=== "Desenvolvimento"

    - [Ambiente de desenvolvimento](development/development-setup.md)
    - [Executando testes](guides/testing.md)
    - [Contribuindo](development/contributing.md)
    - [Documentação do projeto](development/documentation.md)
    - [Declaração de uso de IA](development/ai-usage.md)

## Comando inicial

```bash
git clone https://github.com/thatsrenan/java2c.git
cd java2c
make
./protoparser examples/exemplo1.java
```

Para ver o código C gerado:

```bash
./protoparser --c-clean -o exemplo.c examples/codegen/C01.java
```

O [repositório no GitHub](https://github.com/thatsrenan/java2c) contém o
código-fonte, os exemplos e os manifests de teste.
