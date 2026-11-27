# Padrões e convenções

As convenções abaixo são observadas no código e no Makefile atuais; elas
ajudam a manter a suíte de testes reproduzível.

## Organização e build

- `lexer/lexer.l` contém as regras Flex; `parser/parser.y` contém a gramática
  Bison e as ações que montam a AST e acionam as verificações semânticas.
- `parser/ast.{h,c}` concentra a estrutura da AST, seus construtores, a
  impressão e a liberação.
- Cada etapa posterior tem um par de arquivos em `src/`: tabela de símbolos,
  análise de fluxo, avaliação de constantes, IR, geração da IR, otimização e
  geração de C.
- Flex e Bison geram arquivos em `build/`; o diretório é criado pelo Makefile
  e nunca deve ser versionado. `make clean` remove binários e arquivos gerados.
- `CC`, `CFLAGS` e `SANITIZE` são variáveis do Makefile e podem ser
  sobrescritas na linha de comando, por exemplo
  `make test-ast SANITIZE=`.

## Estilo de código

- O código é C compilado com `gcc -Wall` (`-Wextra` nos testes unitários e
  `-Wno-unused-function` no build principal).
- Nomes de funções e tipos seguem o padrão do módulo: `st_*` para a tabela de
  símbolos, `new_*`/`free_ast`/`print_ast` para a AST, `ir_*` para a
  representação intermediária e `codegen_*` para a geração de C.
- Os tipos da tabela de símbolos usam o prefixo `ST_` (`ST_TYPE_INT`) e as
  categorias usam `SYM_VARIABLE`, `SYM_PARAMETER`, `SYM_GLOBAL` e
  `SYM_FUNCTION`. Documentos antigos em `docs/tabela-simbolos.md` usam nomes
  anteriores (`TYPE_INT`); ao consultá-los, verifique o header atual.
- Mensagens de erro seguem os prefixos usados pelos scripts de teste:
  `ERRO LEXICO na linha N`, `ERRO DE SINTAXE` e
  `ERRO SEMANTICO na linha N`. Não altere esses prefixos sem atualizar os
  manifests e os scripts.
- Ao adicionar uma verificação semântica, inclua um caso em
  `examples/escopo/` e sua expectativa em `esperado.txt`.

## Testes

- Cada conjunto de exemplos tem um manifesto ou arquivos de saída esperada na
  mesma pasta; os scripts em `tests/` fazem a comparação.
- Testes unitários com sanitizers (`asttest`, `ir_opt_test`, `codegen_test`)
  são compilados separadamente do executável principal.
- `make check` é a suíte completa; um novo recurso deve vir acompanhado de um
  alvo ou caso que o exerça. Detalhes em [Executando testes](../guides/testing.md).

## Documentação

- Conteúdo em Markdown dentro de `docs/`; configuração em `mkdocs.yml`.
- Páginas novas precisam entrar no `nav` e usar links relativos com extensão
  `.md`.
- As especificações Markdown em `docs/` foram revisadas em relação ao código;
  o PDF `especificacao-java-subset.pdf` é o único registro histórico divergente
  e deve ser identificado como tal. Ao alterar o compilador, atualize também a
  especificação afetada. O processo está descrito em
  [Documentação do projeto](documentation.md).

## Arquivos versionados

`.gitattributes` fixa terminação LF para `Makefile`, `*.sh` e
`examples/*/esperado.txt`. `.gitignore` exclui binários, `build/` e `site/`.
