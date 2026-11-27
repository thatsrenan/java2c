# Configuração

O protótipo **não possui arquivo de configuração nem variáveis de ambiente
obrigatórias**. Todo o comportamento de execução é escolhido na linha de
comando; as opções disponíveis estão listadas na
[referência da CLI](../reference/cli.md).

## Comportamento na execução

- A entrada é um arquivo `.java` passado como argumento.
- Por padrão, o compilador analisa o arquivo e termina com código `0` apenas
  quando não há erro léxico, sintático ou semântico.
- Opções separadas permitem imprimir a tabela de símbolos, a AST, a
  representação intermediária ou o código C gerado, e escolher o arquivo de
  saída quando aplicável. Mensagens de erro são enviadas para a saída de erro;
  os scripts de teste capturam as duas saídas quando precisam compará-las.
- Não há opção para selecionar dialeto, nível de otimização ou diretório de
  saída padrão além daquelas implementadas em `src/main.c`.

Consulte [Uso do compilador](../guides/usage.md) para exemplos.

## Configuração da compilação

O [Makefile](https://github.com/thatsrenan/java2c/blob/main/Makefile)
centraliza as opções de build:

| Variável | Uso |
| --- | --- |
| `CC` | Compilador C (por padrão, `gcc`) |
| `CFLAGS` | Flags comuns (`-Wall`, diretórios de include) |
| `SANITIZE` | Flags dos testes com sanitizers (AddressSanitizer e UBSan) |

Elas podem ser sobrescritas na chamada, por exemplo:

```bash
make test-ast SANITIZE=
```

## Configuração dos testes

Cada conjunto de exemplos define suas expectativas em arquivos versionados:
`esperado.txt` (léxico, sintaxe e escopo), `*.ast`, `*.tac` ou `*.out`. No
geral, não é necessário alterar scripts para adicionar casos; basta incluir a
entrada e a expectativa correspondente. O formato de cada manifesto está em
[Executando testes](../guides/testing.md).

## Configuração da documentação e publicação

- `mkdocs.yml` define tema, extensões, idioma, navegação e metadados do site.
- `requirements-docs.txt` lista apenas as dependências Python da
  documentação.
- `.github/workflows/documentation.yml` define quando e como o site é
  publicado no GitHub Pages.

A habilitação do Pages no repositório é uma etapa manual do proprietário,
descrita em [Documentação do projeto](../development/documentation.md).
