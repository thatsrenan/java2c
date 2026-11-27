# Ambiente de desenvolvimento

## Pré-requisitos

O projeto é um programa C compilado com GCC; não há gerenciador de pacotes de
aplicação. O [README.md](https://github.com/thatsrenan/java2c/blob/main/README.md)
lista as ferramentas necessárias:

- `gcc` (com suporte a AddressSanitizer e UBSan para os testes com sanitizers);
- `flex` — gera o analisador léxico;
- `bison` — gera o analisador sintático;
- `make` e `git`.

Em Debian/Ubuntu:

```bash
sudo apt update
sudo apt install flex bison build-essential git -y
```

Para trabalhar apenas com a documentação, basta Python com `pip`; as
dependências estão isoladas em `requirements-docs.txt`. Consulte
[Documentação do projeto](documentation.md).

## Preparar o ambiente

```bash
git clone https://github.com/thatsrenan/java2c.git
cd java2c
make
make check
```

`make` gera os executáveis `protoparser` e `lextest` na raiz. Os fontes
gerados por Flex/Bison (`build/lex.yy.c`, `build/parser.tab.c` e
`build/parser.tab.h`) ficam em `build/`, criado automaticamente pelo Makefile,
e não são versionados. Para limpar todos os binários e arquivos gerados:

```bash
make clean
```

## Alvos disponíveis

O [Makefile](https://github.com/thatsrenan/java2c/blob/main/Makefile)
define os seguintes executáveis e alvos de teste:

| Alvo | O que faz |
| --- | --- |
| `all` | Compila `protoparser` e `lextest` (alvo padrão) |
| `protoparser` | Executável principal: léxico, sintaxe, semântica, IR e código C |
| `lextest` | Harness que imprime apenas os tokens reconhecidos |
| `protoparser-san` | Mesmo `protoparser`, compilado com sanitizers |
| `asttest` / `ir_opt_test` / `codegen_test` | Testes unitários compilados separadamente |
| `test-check` | Compara tokens de `examples/lexico` com `esperado.txt` |
| `test-sintaxe` | Compara aceitação/rejeição com `examples/sintaxe/esperado.txt` |
| `test-ast-parser` | Compara `--ast` com os arquivos `.ast` de `examples/ast` |
| `test-ast` | Testa construtores, impressão e liberação da AST |
| `test-mem` | Roda todos os exemplos no binário com sanitizers |
| `test-escopo-check` | Compara erros semânticos com `examples/escopo/esperado.txt` |
| `test-ir-check` | Compara a TAC gerada com `examples/ir/*.tac` |
| `test-ir-opt` | Testa as otimizações de fluxo da IR |
| `test-codegen` | Testa a geração de C a partir de IR construída à mão |
| `test-codegen-check` | Compila e executa os programas de `examples/codegen` ponta a ponta |
| `check` | Executa todas as conferências automáticas acima |

Os alvos `test-lexico` e `test-escopo` mostram entrada e saída para inspeção
manual; `test-all-save` salva relatórios em `build/resultados/`. O alvo
`test-tokens` também existe no Makefile, mas aponta para `examples/tokens/`,
diretório que não existe no repositório; use `make test-check` para conferir
tokens.

Sanitizers são habilitados por padrão nos testes da AST (`SANITIZE =
-fsanitize=address,undefined -fno-omit-frame-pointer`). Se o compilador não
oferecer esse suporte, o Makefile documenta `make test-ast SANITIZE=` para
desabilitá-los.

## Convenções do repositório

`.gitattributes` fixa terminação LF para `Makefile`, scripts `.sh` e arquivos
`esperado.txt`, evitando divergências entre sistemas operacionais. O restante
das convenções está em [Padrões e convenções](conventions.md).

Depois de configurar o ambiente, consulte:

- [Instalação](../getting-started/installation.md) e
  [Início rápido](../getting-started/quick-start.md) para usar `protoparser`;
- [Executando testes](../guides/testing.md) para interpretar falhas;
- [Contribuindo](contributing.md) para o fluxo de pull request.
