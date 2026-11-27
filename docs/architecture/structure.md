# Estrutura do repositório

A árvore abaixo reflete o estado atual do repositório. Arquivos de exemplo são
agrupados por diretório; a listagem completa pode ser obtida com `find . -type f`.

```text
java2c/
├── docs/                              # documentação (MkDocs)
│   ├── index.md                       # página inicial
│   ├── introduction/                  # contexto e escopo
│   ├── getting-started/               # instalação e primeiros passos
│   ├── architecture/                  # esta seção
│   ├── guides/                        # guias de uso e testes
│   ├── reference/                     # CLI e APIs
│   ├── development/                   # desenvolvimento e contribuição
│   ├── definicao-escopo-sintaxe-semantica.md  # especificação original
│   ├── definicao-tokens.md            # especificação léxica original
│   └── tabela-simbolos.md             # documento original (parcialmente obsoleto)
│
├── lexer/
│   └── lexer.l                        # regras Flex: tokens e erros léxicos
├── parser/
│   ├── parser.y                       # gramática Bison e ações semânticas
│   ├── ast.h / ast.c                  # nós da AST, construtores, impressão e liberação
│
├── src/
│   ├── main.c                         # ponto de entrada, CLI e orquestração
│   ├── lextest.c                      # harness isolado do lexer
│   ├── symbol_table.h / symbol_table.c    # escopos, símbolos e checagens de tipos
│   ├── semantic_flow.h / semantic_flow.c  # retorno, alcance e inicialização
│   ├── const_eval.h / const_eval.c        # avaliação de expressões constantes
│   ├── ir.h / ir.c                        # estruturas e impressão da TAC
│   ├── ir_gen.h / ir_gen.c                # AST -> TAC
│   ├── ir_opt.h / ir_opt.c                # otimizações de fluxo na IR
│   └── codegen.h / codegen.c              # IR -> código C
│
├── tests/
│   ├── ast_test.c                     # testes unitários da AST
│   ├── ir_opt_test.c                  # testes do otimizador
│   ├── codegen_test.c                 # testes do gerador de C
│   ├── check.sh                       # comparação de tokens (léxico)
│   ├── check_parser.sh                # aceitação/rejeição (sintaxe)
│   ├── check_ast.sh                   # comparação de árvores `--ast`
│   ├── check_escopo.sh                # erros semânticos esperados
│   ├── check_ir.sh                    # comparação de TAC
│   └── check_mem.sh                   # execução com sanitizers
│
├── examples/
│   ├── exemplo1.java                  # programa usado no README
│   ├── lexico/                        # L01-L56, esperado.txt, LEIA-ME.md
│   ├── sintaxe/                       # S01-S24, esperado.txt
│   ├── ast/                           # A01-A05 (.java e .ast)
│   ├── escopo/                        # E, F, G e T, esperado.txt
│   ├── ir/                            # I01-I11 (.java e .tac)
│   └── codegen/                       # C01-C10 (.java, .out, .clean.out)
│
├── build/                             # gerado por Flex/Bison; não versionado
├── site/                              # gerado por MkDocs; não versionado
├── .github/workflows/documentation.yml # publicação no GitHub Pages
├── mkdocs.yml                         # configuração do MkDocs
├── requirements-docs.txt              # dependências isoladas da documentação
├── Makefile                           # compilação e alvos de teste
├── README.md
├── especificacao-java-subset.pdf      # rascunho histórico da especificação
├── .gitattributes                     # LF para Makefile, scripts e esperado.txt
└── .gitignore
```

## Convenções de localização

- O código-fonte da aplicação permanece em `lexer/`, `parser/` e `src/`. A
  documentação não altera essa organização.
- Arquivos gerados por Flex/Bison ficam em `build/`; o alvo `make clean` remove
  esse diretório e os executáveis da raiz.
- `site/` contém o site estático gerado por `mkdocs build` e também não é
  versionado.
- Cada conjunto de testes tem entradas e resultados esperados na mesma pasta
  de `examples/`, o que permite adicionar casos sem alterar a lógica dos
  scripts. Veja [Executando testes](../guides/testing.md).
