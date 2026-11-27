# Instalação

## Pré-requisitos

Para compilar e executar o protótipo são necessários:

- `gcc` — compilador C;
- `flex` — gerador do analisador léxico;
- `bison` — gerador do analisador sintático;
- `make` e `git`.

Em distribuições Linux baseadas em Debian/Ubuntu, o README recomenda:

```bash
sudo apt update
sudo apt install flex bison build-essential git -y
```

Em outros sistemas, instale as ferramentas equivalentes com o gerenciador de
pacotes da plataforma. Não há arquivos de configuração nem variáveis de
ambiente obrigatórias para executar o compilador; veja
[Configuração](configuration.md).

## Obter o código

```bash
git clone https://github.com/thatsrenan/java2c.git
cd java2c
```

## Compilar

```bash
make
make check
```

O alvo padrão gera `protoparser` (executável principal) e `lextest` (teste
isolado do lexer) na raiz do repositório. Os fontes gerados por Flex/Bison são
colocados em `build/`, que é criado automaticamente e não é versionado.
`make check` executa a suíte completa de conferências; se alguma falhar, a
instalação não está pronta para uso.

Para testar rapidamente:

```bash
./protoparser examples/exemplo1.java
```

O código de saída é `0` somente quando não há erro léxico, sintático ou
semântico.

## Documentação local (opcional)

A documentação tem dependências Python separadas do compilador:

```bash
pip install -r requirements-docs.txt
mkdocs serve
```

Elas não são necessárias para compilar ou executar o java2c. O processo
completo, incluindo a compilação estrita do site, está descrito em
[Documentação do projeto](../development/documentation.md).

## Limpeza

```bash
make clean
```

Remove `protoparser`, `lextest`, os binários de teste, os sanitizers e o
diretório `build/`.

## Próximos passos

- [Início rápido](quick-start.md) — primeiros comandos com `protoparser`.
- [Uso do compilador](../guides/usage.md) — exemplos de todas as opções.
