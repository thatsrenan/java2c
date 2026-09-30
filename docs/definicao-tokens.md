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
* decisões que ainda precisam ser definidas antes da implementação definitiva do analisador léxico.

O analisador léxico deve transformar a sequência de caracteres do código-fonte em uma sequência de tokens que será utilizada posteriormente pelo analisador sintático.

Esta especificação está subordinada ao escopo definido na **Especificação da Linguagem Java Suportada pelo Compilador**.

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
12.34
```

O tratamento de formas numéricas mais avançadas, como notação científica, não faz parte da especificação atual.

---

## 4.2 Literais reais

Literais reais representam valores utilizados nos tipos `float` e `double`.

### Token

```text
REAL_LITERAL
```

### Padrão atualmente adotado

```text
[0-9]+\.[0-9]+
```

### Exemplos

```java
3.14
10.0
7.5
0.25
```

### Observação

A especificação atual não diferencia lexicalmente `float` de `double`.

Portanto, nesta versão:

```text
3.14
```

é reconhecido como `REAL_LITERAL`, cabendo à análise sintática e semântica determinar o contexto e o tipo correspondente.

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

### Decisão pendente

Ainda deve ser decidido se sequências de escape serão suportadas, por exemplo:

```java
'\n'
'\t'
'\\'
'\''
```

Caso escapes não sejam implementados, qualquer ocorrência desse tipo deverá resultar em erro léxico.

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

### Decisões pendentes

Ainda deve ser definido:

* se caracteres de escape serão suportados;
* se `\n`, `\t`, `\"` e `\\` serão aceitos;
* se strings podem conter caracteres Unicode;
* se strings podem conter quebras de linha;
* qual será o comportamento para uma string não terminada.

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

### Decisão pendente

É necessário decidir se `[` e `]` serão reconhecidos pelo analisador léxico:

```text
LBRACKET  [
RBRACKET  ]
```

Mesmo que arrays não sejam suportados em outras partes da linguagem.

### Recomendação

Reconhecer `[` e `]` como tokens e permitir seu uso exclusivamente na assinatura especial de `main`.

Isso evita que o lexer precise tratar `String[] args` como uma construção lexical especial.

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
ERRO LÉXICO: caractere '@' não reconhecido.
```

O erro deve indicar, quando possível:

* o caractere ou lexema inválido;
* a linha;
* a coluna.

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

Exemplo:

```text
ERRO LÉXICO na linha 4, coluna 12:
caractere '@' não reconhecido.
```

A especificação geral do compilador determina que programas com erros léxicos, sintáticos ou semânticos não devem gerar código C.

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
| Literal       | `REAL_LITERAL`    | `[0-9]+\.[0-9]+`         |
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

O lexer deve simplesmente ignorá-los, mantendo a contagem de linha e coluna.

---

# 16. Decisões ainda pendentes

A especificação atual resolve a estrutura principal dos tokens, mas as seguintes decisões ainda precisam ser tomadas antes de considerar a especificação lexical totalmente definitiva.

## 16.1 Literais de caractere

Decidir se serão aceitos escapes:

```java
'\n'
'\t'
'\\'
'\''
```

### Recomendação

Para a primeira versão, não suportar escapes e aceitar apenas um caractere simples.

---

## 16.2 Literais de string

Decidir se serão aceitos escapes:

```java
"\n"
"\t"
"\""
"\\"
```

### Recomendação

Para a primeira versão, manter strings simples e sem escapes.

---

## 16.3 Notação científica

Ainda não está definido se devem ser aceitos valores como:

```java
1e10
2.5e-3
```

### Recomendação

Não suportar inicialmente.

---

## 16.4 Sufixo de literais `float`

Ainda não está definido se será permitido:

```java
3.14f
```

### Recomendação

Não suportar inicialmente.

---

## 16.5 `float` e `double`

Ainda precisa ser definida a relação entre:

```text
REAL_LITERAL
```

e os tipos:

```text
float
double
```

A especificação atual não diferencia lexicalmente os dois.

Essa diferenciação pode ser feita posteriormente na análise semântica.

---

## 16.6 Colchetes

Deve ser tomada uma decisão definitiva sobre:

```text
[
]
```

por causa da assinatura especial:

```java
String[] args
```

### Recomendação

Reconhecê-los como tokens, mas permitir seu uso apenas no contexto especial da declaração de `main`.

---

## 16.7 Unicode

Ainda não está definido se identificadores poderão utilizar caracteres Unicode.

### Recomendação

Para simplificar a implementação inicial com Flex, utilizar identificadores ASCII:

```text
[a-zA-Z_][a-zA-Z0-9_]*
```

---

# 17. Pontos para aprimoramento futuro

As seguintes funcionalidades não são necessárias para concluir a issue atual, mas podem ser consideradas em versões futuras:

* literais numéricos em diferentes bases;
* notação científica;
* escapes em caracteres;
* escapes em strings;
* identificadores Unicode;
* mensagens de erro léxico mais detalhadas;
* recuperação de erros para permitir múltiplos erros em uma única execução;
* armazenamento do lexema e de seu valor convertido na estrutura de token.

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
* [ ] As decisões pendentes da Seção 16 forem formalmente aprovadas pela equipe.

Enquanto as decisões pendentes não forem aprovadas, esta especificação deve ser considerada **versão preliminar**, embora já seja suficiente para orientar a estrutura inicial do analisador léxico.

---

# 19. Relação com as próximas etapas

Esta especificação deve servir como referência para a implementação do analisador léxico com Flex.

O fluxo esperado é:

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

**Status:** preliminar, pronta para implementação inicial do analisador léxico.

A estrutura dos tokens está definida de acordo com o escopo atual da linguagem. Antes da implementação definitiva, a equipe deve aprovar principalmente:

1. suporte a escapes em `char`;
2. suporte a escapes em `String`;
3. notação científica;
4. sufixo `f` para `float`;
5. uso de `[` e `]` na assinatura de `main`;
6. suporte a Unicode em identificadores.

Após essas decisões, esta especificação poderá ser considerada a referência oficial para a implementação do analisador léxico.
