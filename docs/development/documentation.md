# Documentação do projeto

Este site é gerado com [MkDocs](https://www.mkdocs.org/) e
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/). Todo o
conteúdo vive na pasta `docs/`, e a configuração fica em `mkdocs.yml`, na raiz
do repositório.

## Executar localmente

É recomendável usar um ambiente virtual apenas para a documentação, mantendo-a
separada das dependências do compilador:

```bash
python -m venv .venv-docs
source .venv-docs/bin/activate
pip install -r requirements-docs.txt
mkdocs serve
```

O comando `mkdocs serve` inicia um servidor local (por padrão em
`http://127.0.0.1:8000`) e recarrega a página a cada alteração.

Para gerar a versão estática em `site/` com a mesma validação usada na
integração contínua:

```bash
mkdocs build --strict
```

`--strict` transforma avisos — links quebrados, páginas ausentes na navegação,
âncoras inválidas — em erro de compilação. Use-o antes de abrir um pull
request que altere a documentação.

## Organização do conteúdo

```text
docs/
├── index.md                     # página inicial
├── introduction/                # contexto, objetivos, escopo
├── getting-started/             # instalação, configuração, primeiros passos
├── architecture/                # componentes e fluxo do compilador
├── guides/                      # tutoriais e fluxos de trabalho
├── reference/                   # CLI e APIs
├── development/                 # desenvolvimento e contribuição
└── definicao-*.md, tabela-*.md   # especificações revisadas da linguagem
```

!!! note "Especificações revisadas"
    Os arquivos `definicao-escopo-sintaxe-semantica.md`,
    `definicao-tokens.md` e `tabela-simbolos.md` permanecem em `docs/` e estão
    referenciados na seção **Referência**. Foram atualizados para descrever o
    lexer, a tabela de símbolos, a AST, a análise de fluxo e a geração de C
    presentes no repositório. O PDF `especificacao-java-subset.pdf` é o único
    documento divergente e permanece como rascunho histórico.

### Adicionar uma página

1. Crie o arquivo Markdown na subpasta adequada.
2. Adicione-o ao `nav` em `mkdocs.yml`; sem isso a página não aparece na
   navegação lateral.
3. Use links relativos terminados em `.md` (por exemplo,
   `../reference/cli.md`). O MkDocs converte esses links automaticamente.
4. Execute `mkdocs build --strict` e confira se não há avisos.

### Recursos habilitados

| Recurso | Uso |
| --- | --- |
| `admonition` / `pymdownx.details` | blocos de aviso, nota e detalhes recolhíveis |
| `tables` | tabelas em Markdown |
| `toc` com `permalink` | índice de seções com link fixo |
| `pymdownx.highlight` | realce de sintaxe e numeração de linhas |
| `pymdownx.tabbed` | abas quando há variantes de comando |
| `pymdownx.superfences` | diagramas Mermaid em blocos `mermaid` |
| `search` | busca local no site |

Não é necessário instalar um plugin adicional para Mermaid: o tema Material
carrega o renderizador quando encontra um bloco cercado por `mermaid`.

## Publicação no GitHub Pages

O workflow [`.github/workflows/documentation.yml`](https://github.com/thatsrenan/java2c/blob/main/.github/workflows/documentation.yml)
é disparado por:

- `push` na branch `main` que altere `docs/**`, `mkdocs.yml`,
  `requirements-docs.txt` ou o próprio workflow;
- execução manual (`workflow_dispatch`).

Ele instala as dependências, roda `mkdocs build --strict` e publica o conteúdo
de `site/` apenas se a compilação terminar sem erros. A publicação usa as
actions oficiais `configure-pages`, `upload-pages-artifact` e `deploy-pages`,
com permissões mínimas (`contents: read` no checkout e `pages: write` /
`id-token: write` somente onde necessário).

!!! warning "Etapa manual do proprietário"
    Se o GitHub Pages ainda não estiver ativado para o repositório, um
    mantenedor com acesso de administração deve abrir
    **Settings → Pages** e selecionar **Source: GitHub Actions**. Essa
    configuração não pode ser feita por um pull request. Se a organização
    exigir aprovação para o ambiente `github-pages`, a primeira implantação
    também precisará ser aprovada na aba **Environments**.

## Uso de IA na documentação

O uso de IA no projeto, incluindo a produção e revisão destes documentos, está
registrado em [Declaração de uso de IA](ai-usage.md). A página descreve a
abrangência do apoio, a verificação feita contra o código e a responsabilidade
de revisão humana.
