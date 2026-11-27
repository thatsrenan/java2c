# Especificação Léxica da Linguagem Java Suportada pelo Compilador

## 1. Objetivo

Este documento define os tokens reconhecidos pelo analisador léxico do compilador Java para C.

A especificação estabelece:

* as categorias de tokens reconhecidas;
* os lexemas e padrões correspondentes;
* as palavras-chave da linguagem;
* identificadores;
* literais;
* operadores;
* delimitadores;
* comentários;
* espaços em branco;
* regras de prioridade e desambiguação;
* situações de erro léxico;
* decisões consolidadas na implementação atual.

O analisador léxico transforma a sequência de caracteres do código-fonte em uma sequência de tokens utilizada posteriormente pelo analisador sintático.

Esta especificação está subordinada ao escopo definido na **Especificação da Linguagem Java Suportada pelo Compilador**.

> **Estado do documento:** revisado para descrever o `lexer/lexer.l` presente no
> repositório. Decisões antes marcadas como "pendentes" — escapes em `char` e
> string, notação científica, sufixos `f`/`d`, colchetes e limites de inteiro —
> já estão implementadas e registradas abaixo. O PDF
> `especificacao-java-subset.pdf` é um rascunho histórico, não a referência do
> comportamento atual.

---

## 2. Categorias de tokens

Os tokens do subconjunto são divididos nas seguintes categorias:

1. palavras-chave;
2. literais;
3. identificadores;
4. operadores;
5. delimitadores;
6. comentários;
7. espaços em branco.

Comentários e espaços em branco são reconhecidos pelo analisador léxico, mas não devem ser enviados ao analisador sintático.

---

# 3. Palavras-chave

As seguintes palavras-chave fazem parte do subconjunto suportado:

| Token      | Lexema     |
| ---------- | ---------- |
| `PUBLIC`   | `public`   |
| `STATIC`   | `static`   |
| `CLASS`    | `class`    |
| `INT`      | `int`      |
| `FLOAT`    | `float`    |
| `DOUBLE`   | `double`   |
| `BOOLEAN`  | `boolean`  |
| `CHAR`     | `char`     |
| `VOID`     | `void`     |
| `STRING_TYPE` | `String` |
| `IF`       | `if`       |
| `ELSE`     | `else`     |
| `WHILE`    | `while`    |
| `FOR`      | `for`      |
| `DO`       | `do`       |
| `BREAK`    | `break`    |
| `CONTINUE` | `continue` |
| `RETURN`   | `return`   |

Essas palavras correspondem às construções sintáticas atualmente previstas no subconjunto.

---

# 4. Literais

## 4.1 Literais inteiros

Literais inteiros representam valores do tipo `int`.

### Token

```text
INTEGER_LITERAL
```

### Padrão

```text
[0-9]+
```

### Exemplos válidos

```java
0
1
10
123
1000
```

### Exemplos potencialmente inválidos

```java
123abc
```

`123abc` é rejeitado como `literal numerico malformado`. Já `12.34` é um
`REAL_LITERAL` válido. Inteiros são limitados a `INT_MAX`: `2147483648` é
rejeitado, inclusive quando usado como operando de menos unário.

---

## 4.2 Literais reais

Literais reais representam valores utilizados nos tipos `float` e `double`.

### Token

```text
REAL_LITERAL
```

### Padrões atualmente adotados

O lexer reconhece três formas de `REAL_LITERAL` (definições `EXP` e regras em
`lexer/lexer.l`):

```text
[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?[fFdD]?
[0-9]+[eE][+-]?[0-9]+[fFdD]?
[0-9]+[fFdD]
```

### Exemplos

```java
3.14
10.0
7.5
0.25
5.5e10
7E-2
10e3
25d
3.14f
```

### Observação

O sufixo `f`/`F` marca o literal como `float`; os demais são `double`. O lexer
valida o intervalo: `1e999` é erro de overflow e `1e39f` excede `FLT_MAX`.
Expoentes sem dígitos (`1.5e`) e literais malformados (`1.5abc`) também são
erros léxicos.

```text
3.14
```

é reconhecido como `REAL_LITERAL` sem sufixo e tratado como `double`; o contexto
e as regras de promoção definem o tipo final na análise semântica.

---

## 4.3 Literais booleanos

Os valores booleanos suportados são:

```text
true
false
```

### Tokens

```text
TRUE
FALSE
```

### Exemplos

```java
boolean ativo = true;
boolean concluido = false;
```

`true` e `false` não devem ser reconhecidos como identificadores.

---

## 4.4 Literais de caractere

Literais de caractere são utilizados com o tipo `char`.

### Token

```text
CHAR_LITERAL
```

### Forma básica

```text
'caractere'
```

### Exemplos

```java
'A'
'a'
'0'
' '
```

A definição atual considera um único caractere entre aspas simples.

### Escapes suportados

Sequências de escape são suportadas tanto em `char` quanto em string, com os
mesmos caracteres de Java e C (a geração de C pode copiá-las sem tradução):

```java
'\n'
'\t'
'\r'
'\b'
'\f'
'\0'
'\\'
'\''
'"'
```

Um escape fora desse conjunto (`'\q'`) ou um literal com mais de um caractere
(`'ab'`) é erro léxico; o lexer também detecta literal vazio e literal não
terminado.

---

## 4.5 Literais de string

Literais de string são reconhecidos pelo token:

```text
STRING_LITERAL
```

### Forma básica

```text
"texto"
```

### Exemplos

```java
"Olá"
"Resultado:"
"Valor = "
""
```

Literais de string são permitidos exclusivamente na construção simplificada equivalente a:

```java
System.out.println(...)
```

O tipo `String` continua não sendo suportado para declaração de variáveis, parâmetros ou retornos.

Exemplo permitido:

```java
System.out.println("Olá");
```

Exemplo não permitido:

```java
String mensagem = "Olá";
```

### Decisões consolidadas

* Escapes do mesmo conjunto de `char` são aceitos, incluindo `\n`, `\t`,
  `\"` e `\\`. Um escape inválido, como em `"a\qb"`, gera erro léxico.
* Identificadores e literais usam ASCII; bytes não-ASCII fora de literais e
  comentários são rejeitados. Um BOM UTF-8 no início da linha é ignorado.
* Quebras de linha não são permitidas dentro de uma string.
* String não terminada gera `ERRO LEXICO na linha N: string literal nao
  terminada.`

---

# 5. Identificadores

Identificadores representam nomes de:

* variáveis;
* métodos;
* parâmetros;
* campos;
* classes.

### Token

```text
IDENTIFIER
```

### Padrão adotado

```text
[a-zA-Z_][a-zA-Z0-9_]*
```

### Exemplos válidos

```java
x
contador
soma
resultado
_main
valor2
```

### Exemplos inválidos

```java
123valor
2x
nome-variavel
nome@teste
```

Palavras-chave possuem prioridade sobre `IDENTIFIER`.

Por exemplo:

```text
if
```

deve produzir:

```text
IF
```

e não:

```text
IDENTIFIER("if")
```

---

# 6. Operadores

## 6.1 Operadores aritméticos

| Token      | Lexema |
| ---------- | ------ |
| `PLUS`     | `+`    |
| `MINUS`    | `-`    |
| `MULTIPLY` | `*`    |
| `DIVIDE`   | `/`    |
| `MODULO`   | `%`    |

---

## 6.2 Operadores relacionais

| Token           | Lexema |
| --------------- | ------ |
| `EQUAL`         | `==`   |
| `NOT_EQUAL`     | `!=`   |
| `LESS`          | `<`    |
| `GREATER`       | `>`    |
| `LESS_EQUAL`    | `<=`   |
| `GREATER_EQUAL` | `>=`   |

---

## 6.3 Operadores lógicos

| Token | Lexema |
| ----- | ------ |
| `AND` | `&&`   |
| `OR`  | `\|\|` |
| `NOT` | `!`    |

---

## 6.4 Operadores de atribuição

| Token          | Lexema |
| -------------- | ------ |
| `ASSIGN`       | `=`    |
| `PLUS_ASSIGN`  | `+=`   |
| `MINUS_ASSIGN` | `-=`   |
| `MULT_ASSIGN`  | `*=`   |
| `DIV_ASSIGN`   | `/=`   |

---

## 6.5 Incremento e decremento

| Token       | Lexema |
| ----------- | ------ |
| `INCREMENT` | `++`   |
| `DECREMENT` | `--`   |

Os tokens `INCREMENT` e `DECREMENT` podem aparecer tanto na forma prefixada quanto pós-fixada:

```java
++i;
i++;
--i;
i--;
```

A distinção entre prefixo e pós-fixo deve ser realizada pela análise sintática e não pelo analisador léxico.

---

# 7. Delimitadores e símbolos especiais

Os seguintes delimitadores são reconhecidos:

| Token       | Lexema | Utilização                               |
| ----------- | ------ | ---------------------------------------- |
| `LPAREN`    | `(`    | início de parâmetros/expressões          |
| `RPAREN`    | `)`    | fim de parâmetros/expressões             |
| `LBRACE`    | `{`    | início de bloco                          |
| `RBRACE`    | `}`    | fim de bloco                             |
| `SEMICOLON` | `;`    | término de instrução                     |
| `COMMA`     | `,`    | separação de parâmetros                  |
| `DOT`       | `.`    | acesso à construção `System.out.println` |

O ponto (`.`) deve ser reconhecido individualmente, permitindo que o parser reconheça a sequência:

```text
System . out . println
```

A construção completa `System.out.println` não deve ser transformada em um único token pelo analisador léxico.

---

## 7.1 Colchetes

O escopo geral do subconjunto não suporta arrays.

Entretanto, a assinatura convencional do método `main` utiliza:

```java
String[] args
```

O documento de escopo define essa assinatura como uma exceção tratada especialmente.

### Decisão consolidada

`[` e `]` são reconhecidos pelo analisador léxico como `LBRACKET` e `RBRACKET`:

```text
LBRACKET  [
RBRACKET  ]
```

O lexer não precisa de uma regra especial para `String[] args`: a gramática
aceita a sequência `STRING_TYPE LBRACKET RBRACKET IDENTIFIER` apenas como
parâmetro de `main`. Em qualquer outro contexto, colchetes causam erro de
sintaxe. Arrays continuam fora do subconjunto.

---

# 8. Comentários

O subconjunto suporta comentários de linha e de bloco.

## 8.1 Comentário de linha

### Forma

```text
// texto
```

O comentário começa em `//` e termina na próxima quebra de linha ou no fim do arquivo.

Exemplo:

```java
// declaração da variável
int x = 10;
```

O conteúdo do comentário não deve produzir tokens para o analisador sintático.

---

## 8.2 Comentário de bloco

### Forma

```text
/* texto */
```

Exemplo:

```java
/*
 * comentário
 * de múltiplas linhas
 */
int x = 10;
```

Comentários de bloco podem ocupar múltiplas linhas.

Um comentário de bloco sem `*/` deve resultar em erro léxico.

Exemplo:

```java
/*
int x = 10;
```

Resultado esperado:

```text
ERRO LÉXICO: comentário de bloco não terminado.
```

---

# 9. Espaços em branco

São considerados espaços em branco:

```text
espaço
tabulação
quebra de linha
retorno de carro
```

Esses caracteres devem ser ignorados pelo analisador léxico quando não fizerem parte de um literal ou comentário.

Exemplo:

```java
int x = 10;
```

e:

```java
int    x
    =
    10
    ;
```

devem produzir a mesma sequência de tokens.

Apesar de serem descartadas, quebras de linha devem ser contabilizadas para permitir a localização de erros.

---

# 10. Regra de prioridade e desambiguação

O analisador léxico deve utilizar a regra de **maior correspondência** (*maximal munch*): quando diferentes tokens podem corresponder ao mesmo início da entrada, deve ser selecionado o token que consome a maior quantidade de caracteres.

### Exemplos

A entrada:

```text
==
```

deve produzir:

```text
EQUAL
```

e não:

```text
ASSIGN ASSIGN
```

A entrada:

```text
<=
```

deve produzir:

```text
LESS_EQUAL
```

e não:

```text
LESS ASSIGN
```

A entrada:

```text
++
```

deve produzir:

```text
INCREMENT
```

e não:

```text
PLUS PLUS
```

A entrada:

```text
+=
```

deve produzir:

```text
PLUS_ASSIGN
```

e não:

```text
PLUS ASSIGN
```

---

## 10.1 Prioridade das palavras-chave

Palavras-chave devem possuir prioridade sobre identificadores.

Por exemplo:

```text
while
```

deve gerar:

```text
WHILE
```

e não:

```text
IDENTIFIER("while")
```

Entretanto:

```text
whileLoop
```

deve gerar:

```text
IDENTIFIER("whileLoop")
```

---

# 11. Símbolos não reconhecidos

Qualquer caractere que não pertença a um token válido e que não seja espaço em branco ou parte de um comentário deve gerar um erro léxico.

Exemplo:

```java
int x = 10 @ 5;
```

Resultado esperado:

```text
ERRO LEXICO na linha 1: caractere '@' nao reconhecido.
```

A mensagem indica o caractere ou lexema inválido e a linha (`yylineno`). O
lexer não reporta coluna; a localização usada pelo restante do compilador é a
linha, inclusive nas anotações `@linha` da AST.

---

# 12. Casos inválidos relevantes

Os seguintes casos devem ser tratados como erros ou encaminhados para validação pelas etapas posteriores.

## 12.1 Identificador iniciado por número

```java
int 123abc;
```

Deve ser rejeitado.

---

## 12.2 Caractere isolado inválido

```java
int x = 10 @ 2;
```

O caractere `@` não pertence ao subconjunto e deve gerar erro léxico.

---

## 12.3 String não terminada

```java
System.out.println("Olá);
```

Deve gerar erro léxico.

---

## 12.4 Caractere não terminado

```java
char c = 'A;
```

Deve gerar erro léxico.

---

## 12.5 Comentário de bloco não terminado

```java
/*
int x = 10;
```

Deve gerar erro léxico.

---

## 12.6 Literal numérico malformado

```java
double x = 3.14.15;
```

Deve ser rejeitado.

---

## 12.7 Operador não suportado

Exemplo:

```java
x &= 10;
```

O operador `&=` não pertence ao conjunto suportado e deve ser rejeitado.

---

# 13. Tratamento de erros léxicos

Quando um erro léxico for encontrado, o analisador deve:

1. identificar o caractere ou lexema problemático;
2. informar sua localização, quando possível;
3. emitir uma mensagem de erro;
4. sinalizar que a análise léxica falhou;
5. impedir a geração de código C para o programa inválido.

Na implementação atual, o lexer escreve em `stderr`, usa o prefixo `ERRO
LEXICO na linha N`, marca `had_lexical_error = 1`, descarta o token inválido e
continua a análise. `main.c` verifica essa flag depois de `yyparse()` e impede
a geração de IR e de C mesmo quando o parser termina "com sucesso".

Exemplo real:

```text
ERRO LEXICO na linha 1: caractere '@' nao reconhecido.
```

Programas com erros léxicos, sintáticos ou semânticos não geram código C e
terminam com código de saída `1`.

---

# 14. Tabela consolidada de tokens

| Categoria     | Token             | Lexema/Padrão            |
| ------------- | ----------------- | ------------------------ |
| Palavra-chave | `PUBLIC`          | `public`                 |
| Palavra-chave | `STATIC`          | `static`                 |
| Palavra-chave | `CLASS`           | `class`                  |
| Palavra-chave | `INT`             | `int`                    |
| Palavra-chave | `FLOAT`           | `float`                  |
| Palavra-chave | `DOUBLE`          | `double`                 |
| Palavra-chave | `BOOLEAN`         | `boolean`                |
| Palavra-chave | `CHAR`            | `char`                   |
| Palavra-chave | `VOID`            | `void`                   |
| Palavra-chave | `STRING_TYPE`     | `String`                 |
| Palavra-chave | `IF`              | `if`                     |
| Palavra-chave | `ELSE`            | `else`                   |
| Palavra-chave | `WHILE`           | `while`                  |
| Palavra-chave | `FOR`             | `for`                    |
| Palavra-chave | `DO`              | `do`                     |
| Palavra-chave | `BREAK`           | `break`                  |
| Palavra-chave | `CONTINUE`        | `continue`               |
| Palavra-chave | `RETURN`          | `return`                 |
| Literal       | `TRUE`            | `true`                   |
| Literal       | `FALSE`           | `false`                  |
| Literal       | `INTEGER_LITERAL` | `[0-9]+`                 |
| Literal       | `REAL_LITERAL`    | decimal, expoente e sufixo `f`/`d` (ver §4.2) |
| Literal       | `CHAR_LITERAL`    | `'caractere'`            |
| Literal       | `STRING_LITERAL`  | `"texto"`                |
| Identificador | `IDENTIFIER`      | `[a-zA-Z_][a-zA-Z0-9_]*` |
| Operador      | `PLUS`            | `+`                      |
| Operador      | `MINUS`           | `-`                      |
| Operador      | `MULTIPLY`        | `*`                      |
| Operador      | `DIVIDE`          | `/`                      |
| Operador      | `MODULO`          | `%`                      |
| Operador      | `EQUAL`           | `==`                     |
| Operador      | `NOT_EQUAL`       | `!=`                     |
| Operador      | `LESS`            | `<`                      |
| Operador      | `GREATER`         | `>`                      |
| Operador      | `LESS_EQUAL`      | `<=`                     |
| Operador      | `GREATER_EQUAL`   | `>=`                     |
| Operador      | `AND`             | `&&`                     |
| Operador      | `OR`              | `\|\|`                   |
| Operador      | `NOT`             | `!`                      |
| Operador      | `ASSIGN`          | `=`                      |
| Operador      | `PLUS_ASSIGN`     | `+=`                     |
| Operador      | `MINUS_ASSIGN`    | `-=`                     |
| Operador      | `MULT_ASSIGN`     | `*=`                     |
| Operador      | `DIV_ASSIGN`      | `/=`                     |
| Operador      | `INCREMENT`       | `++`                     |
| Operador      | `DECREMENT`       | `--`                     |
| Delimitador   | `LPAREN`          | `(`                      |
| Delimitador   | `RPAREN`          | `)`                      |
| Delimitador   | `LBRACE`          | `{`                      |
| Delimitador   | `RBRACE`          | `}`                      |
| Delimitador   | `SEMICOLON`       | `;`                      |
| Delimitador   | `COMMA`           | `,`                      |
| Delimitador   | `DOT`             | `.`                      |
| Delimitador   | `LBRACKET`        | `[`                      |
| Delimitador   | `RBRACKET`        | `]`                      |

---

# 15. Tokens descartados pelo lexer

Os seguintes elementos não devem ser enviados ao analisador sintático:

### Espaços em branco

```text
' '
'\t'
'\n'
'\r'
```

### Comentários

```text
// ...
```

e:

```text
/* ... */
```

O lexer simplesmente os ignora e mantém a contagem de linha (`yylineno`),
usada tanto nas mensagens de erro quanto nas anotações `@linha` da AST.

---

# 16. Decisões consolidadas na implementação

As decisões que antes apareciam como pendentes foram resolvidas no
`lexer/lexer.l` atual:

| Tema | Decisão implementada |
| --- | --- |
| Escapes em `char` e string | Aceitos `\n`, `\t`, `\r`, `\b`, `\f`, `\0`, `\\`, `\'` e `\"`; escape fora desse conjunto é erro léxico |
| Notação científica | Aceita em reais com ou sem parte decimal (`1e10`, `2.5e-3`) |
| Sufixo `f`/`d` | Aceito em reais; `f`/`F` marca `float`, `d`/`D` e a ausência de sufixo marcam `double` |
| Colchetes | Reconhecidos como `LBRACKET`/`RBRACKET`; a gramática permite `String[] args` apenas em `main` |
| Identificadores Unicode | Não suportados: `[a-zA-Z_][a-zA-Z0-9_]*` |
| Intervalo de reais | `strtod` com verificação de infinito e de `FLT_MAX` quando há sufixo `f` |
| Intervalo de inteiros | Limitado a `INT_MAX` |

As seções 4.1, 4.2, 4.4, 4.5 e 7.1 registram os padrões e mensagens
correspondentes.

---

# 17. Pontos para aprimoramento futuro

Notação científica, escapes, sufixos de reais e colchetes já foram
implementados e saíram desta lista (ver seção 16). Restam como extensões
possíveis:

* literais numéricos em diferentes bases (hexadecimal, octal, binário);
* identificadores Unicode;
* recuperação de erros para reportar mais de um erro léxico em uma única
  execução (hoje o lexer descarta o token inválido e continua);
* mensagens ainda mais específicas para literais numéricos malformados.

Essas funcionalidades não devem ser implementadas sem antes alterar formalmente o escopo da linguagem.

---

# 18. Critérios de aceitação da issue

A issue **“Definir tokens da linguagem-fonte”** será considerada concluída quando:

* [x] As categorias de tokens estiverem definidas.
* [x] As palavras-chave suportadas estiverem listadas.
* [x] Os identificadores possuírem um padrão definido.
* [x] Os literais inteiros possuírem um padrão definido.
* [x] Os literais reais possuírem um padrão definido.
* [x] Os literais booleanos estiverem definidos.
* [x] Os literais de caractere estiverem definidos.
* [x] Os literais de string estiverem definidos.
* [x] Os operadores suportados estiverem listados individualmente.
* [x] Os delimitadores suportados estiverem listados.
* [x] Os comentários suportados estiverem definidos.
* [x] Os espaços em branco estiverem definidos.
* [x] A regra de maior correspondência estiver definida.
* [x] A prioridade das palavras-chave sobre identificadores estiver definida.
* [x] Os principais casos de erro léxico estiverem documentados.
* [x] O comportamento geral diante de erro léxico estiver definido.
* [x] As decisões da Seção 16 estiverem implementadas no analisador léxico.

A especificação deixou de ser preliminar: o lexer reconhece os tokens descritos
aqui e a suíte `make test-check` verifica os casos de `examples/lexico/`.

---

# 19. Relação com as próximas etapas

Esta especificação serve como referência para o analisador léxico com Flex já
implementado em `lexer/lexer.l`.

O fluxo implementado é:

```text
Código Java
    ↓
Analisador Léxico
    ↓
Tokens
    ↓
Analisador Sintático
    ↓
AST / estrutura sintática
    ↓
Análise Semântica
    ↓
Geração de C
```

O analisador léxico não deve realizar validações que pertencem à análise sintática ou semântica.

Por exemplo:

```java
int x = true;
```

contém tokens lexicalmente válidos:

```text
INT IDENTIFIER ASSIGN TRUE SEMICOLON
```

A incompatibilidade entre `int` e `boolean` deve ser detectada posteriormente pela análise semântica.

Da mesma forma, uma sequência como:

```java
if (x > 0)
```

pode ser lexicalmente válida mesmo que a gramática posteriormente determine que falta um bloco ou outra construção obrigatória.

---

# 20. Estado da especificação

**Status:** implementada e revisada em relação ao `lexer/lexer.l` atual.

A estrutura dos tokens corresponde ao escopo da linguagem descrito na
[especificação de escopo e sintaxe](definicao-escopo-sintaxe-semantica.md). As
decisões antes listadas como pendentes foram resolvidas: escapes, notação
científica, sufixos `f`/`d`, colchetes em `String[] args`, limites numéricos e
identificadores ASCII (sem Unicode). A verificação automática está em
`make test-check`, com os manifestos em `examples/lexico/`.

Para o estado consolidado do projeto, consulte a
[visão geral](introduction/overview.md) e a
[arquitetura](architecture/overview.md).
