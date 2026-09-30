# java2c

## Estrutura do Repositório

```
.java2c
│
├── lexer
│   └── lexer.l
├── parser
│   └── parser.y
├── src
│   └── main.c
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

## Como Contribuir

1. Faça um _fork_ do repositório.
2. Crie uma _branch_ para suas alterações:

``` bash
git checkout -b minha-feature
```

3. Faça _commit_ e envie um _pull request_.