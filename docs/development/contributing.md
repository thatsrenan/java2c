# Contribuindo

O fluxo de contribuição registrado no [README.md](https://github.com/thatsrenan/java2c/blob/main/README.md)
é o usual de fork e pull request:

1. Faça um _fork_ do repositório.
2. Crie uma branch para suas alterações:

   ```bash
   git checkout -b minha-feature
   ```

3. Faça o _commit_ e envie um pull request.

## Antes de abrir o pull request

- Compile e rode a suíte de conferências automáticas:

  ```bash
  make
  make check
  ```

  `make check` reúne os testes de léxico, AST, otimizador de IR, gerador de C,
  sintaxe, AST do parser, memória, escopo/semântica, IR e geração de código de
  ponta a ponta. Veja [Executando testes](../guides/testing.md) para o detalhamento.

- Se a alteração modificar comportamento visível do executável, atualize a
  [referência da linha de comando](../reference/cli.md) e os
  [exemplos de uso](../guides/usage.md).
- Se a alteração modificar estruturas ou funções das APIs, atualize as páginas
  de [referência](../reference/ast-api.md).
- Se você alterou a documentação, valide-a localmente:

  ```bash
  pip install -r requirements-docs.txt
  mkdocs build --strict
  ```

## Convenções a respeitar

- O código-fonte permanece em `src/`, `lexer/` e `parser/`; arquivos gerados
  por Flex/Bison ficam em `build/` e não devem ser versionados. Consulte
  [Padrões e convenções](conventions.md).
- Novas páginas precisam ser adicionadas ao `nav` em `mkdocs.yml`, como
  descrito em [Documentação do projeto](documentation.md).
- A documentação descreve o comportamento implementado. As especificações
  Markdown devem ser atualizadas junto com o código; apenas o PDF
  `especificacao-java-subset.pdf` é tratado como registro histórico.
