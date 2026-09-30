# Especificação da Linguagem Java Suportada pelo Compilador (Java para C)

## ESCOPO E SINTAXE

## 1. Escopo do subconjunto

Este documento define o comportamento esperado do subconjunto de Java aceito como entrada pelo compilador, cujo objetivo é traduzir código-fonte Java para C.

Como C não possui orientação a objetos, o subconjunto adotado é predominantemente procedural, utilizando métodos `static` Java como equivalentes a funções em C. A classe Java serve principalmente como estrutura sintática obrigatória da linguagem, sendo removida conceitualmente durante a geração do código C.

O escopo foi definido de forma a contemplar construções suficientes para demonstrar o funcionamento completo do compilador, desde a análise léxica e sintática até a análise semântica e a geração de código C.

### 1.1 Funcionalidades suportadas

O compilador suporta as seguintes funcionalidades do subconjunto Java:

* Tipos primitivos:

  * `int`;
  * `float`;
  * `double`;
  * `boolean`;
  * `char`;
  * `void`.
* Declaração e atribuição de variáveis.
* Campos `static`, traduzidos para variáveis globais em C.
* Operadores aritméticos, relacionais, lógicos e de atribuição.
* Operadores de incremento e decremento.
* Expressões com precedência e associatividade.
* Conversões de tipos compatíveis com o escopo definido.
* Estruturas condicionais:

  * `if`;
  * `else if`;
  * `else`.
* Estruturas de repetição:

  * `while`;
  * `for`;
  * `do while`.
* Comandos `break` e `continue` dentro de estruturas de repetição.
* Comando `return`.
* Métodos `static` com parâmetros e valor de retorno.
* Método `main`.
* Passagem de parâmetros por valor para tipos primitivos.
* Impressão de valores utilizando uma construção simplificada equivalente a `System.out.println`.
* Literais de string utilizados exclusivamente na construção simplificada de impressão.
* Comentários de linha e de bloco.
* Escopo de bloco e de método.
* Tipagem estática e regras básicas de verificação de tipos.

### 1.2 Funcionalidades fora do escopo

As seguintes funcionalidades não fazem parte do subconjunto suportado:

* Arrays;
* Tipo `String` como tipo de variável ou parâmetro;
* Herança e polimorfismo;
* Interfaces;
* Classes abstratas;
* Generics;
* Sobrecarga de métodos (overloading);
* Tratamento de exceções (`try`, `catch`, `finally`, `throw`);
* Threads e concorrência;
* Anotações;
* Reflection;
* Lambda expressions;
* Streams;
* Coleções (`List`, `Map`, etc.);
* Bibliotecas externas;
* Recursos avançados da biblioteca padrão do Java;
* Gerenciamento de memória e características específicas da JVM;
* Ponteiros e referências;
* Boxing e unboxing;
* Recursos de orientação a objetos que dependam de instâncias de classes.

### 1.3 Justificativa para a ausência de arrays

Arrays não fazem parte da versão inicial do subconjunto.

A exclusão de arrays não ocorre por impossibilidade de tradução para C, mas pela necessidade de manter o escopo do compilador controlado. Arrays introduziriam construções adicionais na análise léxica, sintática e semântica, como declaração de tipos indexados, criação de arrays, acesso por índice e atribuição a posições.

Além disso, seria necessário definir regras específicas para a representação de arrays Java na geração de código C, incluindo inicialização, tamanho e operações de acesso.

Como arrays não são necessários para demonstrar o fluxo completo:

```text
Java
  ↓
Análise léxica
  ↓
Análise sintática
  ↓
Análise semântica
  ↓
Geração de C
```

eles foram deixados fora do escopo inicial. Sua implementação poderá ser considerada como uma extensão futura.

Exemplo de construção não suportada:

```java
int[] numeros = new int[10];
```

Resultado esperado:

```text
ERRO: arrays não são suportados pelo subconjunto Java.
```

### 1.4 Justificativa para a ausência do tipo `String`

`String` não faz parte dos tipos suportados pelo subconjunto, pois em Java `String` é uma classe da biblioteca padrão e não um tipo primitivo.

O subconjunto adotado utiliza exclusivamente tipos primitivos para variáveis, parâmetros e valores de retorno:

```text
int
float
double
boolean
char
void
```

A inclusão de `String` como tipo exigiria decisões adicionais sobre representação, armazenamento, comparação, concatenação, acesso a caracteres e métodos da classe `String`, aumentando significativamente o escopo da implementação.

Entretanto, literais de string são permitidos exclusivamente na construção simplificada equivalente a `System.out.println`, permitindo a impressão de mensagens textuais sem transformar `String` em um tipo da linguagem.

Exemplo permitido:

```java
System.out.println("Olá");
```

Exemplo não permitido:

```java
String nome = "Renan";
```

Resultado esperado:

```text
ERRO: o tipo String não é suportado pelo subconjunto Java.
```

---

## 2. Tipos

O compilador suporta apenas os seguintes tipos:

| Tipo Java | Tipo C gerado | Observação                                                                         |
| --------- | ------------- | ---------------------------------------------------------------------------------- |
| `int`     | `int`         | 32 bits                                                                            |
| `float`   | `float`       | 32 bits                                                                            |
| `double`  | `double`      | 64 bits                                                                            |
| `char`    | `char`        | O subconjunto utiliza caracteres compatíveis com o mapeamento adotado pelo gerador |
| `boolean` | `int`         | Convenção `0` para `false` e `1` para `true`                                       |
| `void`    | `void`        | Apenas como tipo de retorno de método                                              |

### Sintaxe

```text
<tipo> ::= "int"
         | "float"
         | "double"
         | "char"
         | "boolean"
         | "void"
```

### Semântica

* Todo tipo utilizado por variáveis e parâmetros é primitivo.
* Não são suportados tipos de referência.
* Não há boxing ou unboxing automático.
* Não há inferência de tipo.
* O uso de `var` não é permitido.
* `String` não é considerado um tipo válido.
* Arrays não são considerados tipos válidos.
* `void` só pode ser utilizado como tipo de retorno de métodos.

### Exemplos

```java
int idade;
float altura;
double salario;
char letra;
boolean ativo;
```

Exemplos inválidos:

```java
String nome;
int[] valores;
var numero = 10;
```

---

## 3. Variáveis

### Sintaxe

```text
<declaracao_variavel> ::= <tipo> <identificador>
                           ["=" <expressao>] ";"
```

### Semântica

* Toda variável precisa ser declarada com um tipo explícito antes do uso.
* A linguagem possui tipagem estática.
* Variáveis locais devem ser inicializadas antes de serem utilizadas.
* Uma variável local é visível dentro do bloco `{ }` em que foi declarada e nos blocos aninhados.
* Variáveis locais não são visíveis fora do método em que foram declaradas.

### Exemplos

```java
int contador = 0;
double media;
media = 7.5;
char letra = 'A';
boolean ativo = true;
```

---

## 3.1 Variáveis globais

Java não possui variáveis globais no sentido de C. O equivalente mais próximo é um campo `static`, compartilhado pelos métodos `static` da classe.

O compilador suporta a tradução de campos `static` Java para variáveis globais em C.

### Sintaxe

```text
<campo_static> ::= "static" <tipo> <identificador>
                   ["=" <expressao>] ";"
```

### Semântica

* Todo campo `static` aceito como entrada é traduzido para uma variável global em C.
* Todo método do subconjunto é `static`.
* Todo campo do subconjunto deve ser `static`.
* Um método `static` pode acessar diretamente os campos `static`.
* Como todos os campos e métodos possuem comportamento estático, o mapeamento para variáveis globais e funções C preserva o comportamento sem necessidade de implementar instâncias de objetos.
* Campos `static` não inicializados recebem os valores padrão correspondentes.

### Exemplo

#### Java

```java
public class Main {
    static int contador = 0;

    static void incrementa() {
        contador++;
    }

    public static void main(String[] args) {
        incrementa();
        incrementa();
        System.out.println(contador);
    }
}
```

#### C gerado

```c
#include <stdio.h>

int contador = 0;

void incrementa() {
    contador++;
}

int main() {
    incrementa();
    incrementa();
    printf("%d\n", contador);
    return 0;
}
```

---

## 4. Operadores

| Categoria             | Operadores suportados | Exemplo          |    |                  |
| --------------------- | --------------------- | ---------------- | -- | ---------------- |
| Aritméticos           | `+ - * / %`           | `a + b`, `a % b` |    |                  |
| Relacionais           | `== != < > <= >=`     | `a >= b`         |    |                  |
| Lógicos               | `&&                   |                  | !` | `a > 0 && b > 0` |
| Atribuição            | `= += -= *= /=`       | `x += 1`         |    |                  |
| Incremento/decremento | `++ --`               | `i++`, `--j`     |    |                  |

### Semântica

* `/` entre dois valores inteiros realiza divisão inteira.
* `/` entre valores `float` ou `double` realiza divisão real.
* `%` é permitido apenas para tipos inteiros.
* `&&` e `||` utilizam avaliação com curto-circuito.
* `++` e `--` podem ser utilizados nas formas pré-fixada e pós-fixada.
* Operadores possuem precedência e associatividade compatíveis com as regras definidas para o subconjunto.

### Exemplo

```java
int soma = a + b;
int resto = a % 2;
boolean valido = (a > 0) && (b != 0);
```

---

## 5. Expressões

### Sintaxe simplificada

```text
<expressao> ::= <termo> ((<op_bin>) <termo>)*

<termo> ::= <literal>
          | <identificador>
          | <chamada_metodo>
          | "(" <expressao> ")"
```

### Semântica

A precedência dos operadores segue a seguinte ordem:

```text
()
!
* / %
+ -
< <= > >= == !=
&&
||
=
```

Expressões são avaliadas respeitando precedência e associatividade.

Toda expressão possui um tipo estático, que deve ser verificado durante a análise semântica.

Caso uma expressão possua tipos incompatíveis, a compilação deve falhar e nenhum código C deve ser gerado.

### Exemplos

```java
int resultado = (a + b) * 2 - c / 2;

boolean condicao = (x > 0) && (y < 10 || z == 0);
```

---

## 6. Estruturas condicionais

### Sintaxe

```text
<if_stmt> ::= "if" "(" <expressao_booleana> ")" <bloco>
              ["else" (<if_stmt> | <bloco>)]
```

### Semântica

* A condição deve possuir tipo `boolean`.
* `else if` é representado por um `if` aninhado no `else`.
* Na geração para C, `boolean` é representado por `int`, utilizando `0` para falso e `1` para verdadeiro.

### Exemplo

```java
if (idade >= 18) {
    System.out.println("Maior de idade");
} else if (idade >= 12) {
    System.out.println("Adolescente");
} else {
    System.out.println("Crianca");
}
```

---

## 7. Estruturas de repetição

O subconjunto suporta:

* `while`;
* `for`;
* `do while`.

### 7.1 While

#### Sintaxe

```text
<while_stmt> ::= "while" "(" <expressao_booleana> ")" <bloco>
```

#### Semântica

A condição deve possuir tipo `boolean`.

O corpo do `while` é executado enquanto a condição for verdadeira.

#### Exemplo

```java
int i = 0;

while (i < 10) {
    System.out.println(i);
    i++;
}
```

---

### 7.2 For

#### Sintaxe

```text
<for_stmt> ::= "for" "(" [<init>] ";"
               [<expressao_booleana>] ";"
               [<incremento>] ")" <bloco>
```

#### Semântica

O `for` possui:

1. inicialização;
2. condição;
3. incremento.

A sintaxe equivalente em C permite uma tradução direta.

#### Exemplo

```java
for (int i = 0; i < 10; i++) {
    System.out.println(i);
}
```

Uma construção equivalente utilizando `while` é:

```java
int i = 0;

while (i < 10) {
    System.out.println(i);
    i++;
}
```

---

### 7.3 Do while

O `do while` faz parte do subconjunto suportado.

#### Sintaxe

```text
<do_while_stmt> ::= "do" <bloco>
                    "while" "(" <expressao_booleana> ")" ";"
```

#### Semântica

O corpo do `do while` é executado pelo menos uma vez. Após cada execução, a condição é avaliada. A repetição continua enquanto a condição for verdadeira.

A tradução para C é direta, pois C possui a mesma estrutura de repetição.

#### Exemplo

```java
int i = 0;

do {
    System.out.println(i);
    i++;
} while (i < 10);
```

Geração equivalente em C:

```c
int i = 0;

do {
    printf("%d\n", i);
    i++;
} while (i < 10);
```

---

### 7.4 Break e continue

Os comandos `break` e `continue` são suportados dentro de estruturas de repetição.

#### Exemplo

```java
while (x < 10) {
    if (x == 5) {
        break;
    }

    x++;
}
```

```java
for (int i = 0; i < 10; i++) {
    if (i % 2 == 0) {
        continue;
    }

    System.out.println(i);
}
```

---

## 8. Métodos

Como o subconjunto não utiliza orientação a objetos baseada em instâncias, todo método aceito deve ser `static` e é traduzido para uma função C.

### Sintaxe

```text
<metodo> ::= ["public"] "static" <tipo_retorno>
             <identificador> "(" [<parametros>] ")" <bloco>

<parametros> ::= <tipo> <identificador>
                 ("," <tipo> <identificador>)*
```

### Semântica

* Todo método aceito como entrada deve ser `static`.
* Não existem métodos associados a instâncias de objetos.
* Parâmetros de tipos primitivos são passados por valor.
* Métodos com retorno diferente de `void` devem possuir `return`.
* Todos os caminhos de execução de um método com retorno devem produzir um valor.
* Sobrecarga de métodos não é suportada.
* Dois métodos não podem possuir o mesmo nome dentro da mesma classe, independentemente de seus parâmetros.

### Exemplo

```java
public static int soma(int a, int b) {
    return a + b;
}
```

A tradução para C é:

```c
int soma(int a, int b) {
    return a + b;
}
```

### Overloading

Sobrecarga de métodos não faz parte do subconjunto.

O seguinte código deve ser rejeitado:

```java
static int soma(int a, int b) {
    return a + b;
}

static double soma(double a, double b) {
    return a + b;
}
```

Resultado esperado:

```text
ERRO: sobrecarga de métodos não é suportada pelo subconjunto Java.
```

A decisão evita a necessidade de realizar name mangling ou outro mecanismo para representar funções com o mesmo nome em C.

---

## 8.1 Método main

O programa Java deve possuir um método `main` responsável pelo início da execução.

A estrutura esperada é:

```java
public class Main {
    public static void main(String[] args) {
        // código
    }
}
```

No subconjunto, o parâmetro `String[] args` possui tratamento especial por fazer parte da assinatura convencional do método `main`.

O uso geral de `String` e de arrays continua fora do escopo.

O método `main` é traduzido para a função `main` em C.

Exemplo:

```java
public static void main(String[] args) {
    int x = 10;
    System.out.println(x);
}
```

Pode gerar:

```c
int main() {
    int x = 10;
    printf("%d\n", x);
    return 0;
}
```

---

## 9. Impressão

O subconjunto utiliza uma construção simplificada equivalente a `System.out.println`.

A construção pode receber valores dos tipos suportados e literais de string.

### Exemplos

```java
System.out.println(10);
System.out.println(x);
System.out.println(3.14);
System.out.println('A');
System.out.println(true);
System.out.println("Olá");
```

Literais de string são permitidos apenas nesse contexto.

A construção:

```java
String mensagem = "Olá";
```

não é suportada.

### Geração para C

Exemplo:

```java
System.out.println("Olá");
```

pode ser traduzido para:

```c
printf("%s\n", "Olá");
```

Exemplo:

```java
System.out.println(x);
```

pode ser traduzido para uma chamada `printf` com o especificador correspondente ao tipo de `x`.

---

## 10. Regras de escopo

### 10.1 Escopo de bloco

Uma variável declarada dentro de `{ }` só existe dentro daquele bloco e dos blocos aninhados dentro dele.

Exemplo:

```java
{
    int x = 10;

    if (x > 0) {
        int y = 20;
        System.out.println(y);
    }
}
```

`y` não pode ser acessada fora do bloco em que foi declarada.

### 10.2 Escopo de método

Parâmetros e variáveis locais de um método não são visíveis fora dele.

### 10.3 Escopo de classe

Campos `static` são traduzidos para variáveis globais em C.

### 10.4 Regra de acesso static

No subconjunto adotado:

* todo método é `static`;
* todo campo é `static`;
* não existem campos associados a instâncias;
* não existem métodos associados a instâncias.

Isso permite que campos `static` Java sejam traduzidos diretamente para variáveis globais em C.

### Exemplo

```java
public class Main {
    static int contador = 0;

    static void incrementar() {
        contador++;
    }

    public static void main(String[] args) {
        incrementar();
        System.out.println(contador);
    }
}
```

O campo `contador` pode ser traduzido diretamente para uma variável global em C.

---

## 11. Regras básicas de tipagem

O subconjunto utiliza tipagem estática.

Toda variável e expressão possui um tipo conhecido durante a compilação.

### 11.1 Conversões implícitas

São permitidas conversões compatíveis com o conjunto de tipos adotado, respeitando a direção de promoção definida pelo compilador.

Exemplo:

```java
int x = 10;
double y = x;
```

### 11.2 Conversões explícitas

Conversões que possam causar perda de informação exigem cast explícito.

Exemplo:

```java
double d = 3.9;
int i = (int) d;
```

### 11.3 Condições booleanas

Expressões utilizadas como condições em:

* `if`;
* `while`;
* `for`;
* `do while`;

devem possuir tipo `boolean`.

Exemplo válido:

```java
if (x > 0) {
    System.out.println(x);
}
```

Exemplo inválido:

```java
int x = 10;

if (x) {
    System.out.println(x);
}
```

Resultado esperado:

```text
ERRO: a condição deve possuir tipo boolean.
```

### 11.4 Tipos incompatíveis

Não são permitidas conversões automáticas entre tipos incompatíveis.

Exemplo:

```java
boolean ativo = true;
int x = ativo;
```

Resultado esperado:

```text
ERRO: tipos incompatíveis.
```

---

## 12. Comentários

São suportados comentários de linha e de bloco.

### Comentário de linha

```java
// Este é um comentário
int x = 10;
```

### Comentário de bloco

```java
/*
 * Este é um comentário
 * de múltiplas linhas.
 */
int x = 10;
```

Comentários não produzem código correspondente na saída C.

---

## 13. Construções não suportadas e comportamento esperado

Qualquer construção que não faça parte da gramática ou das regras semânticas do subconjunto deve ser rejeitada pelo compilador.

Quando possível, a mensagem de erro deve indicar:

* tipo do erro;
* construção inválida;
* localização do erro no código-fonte.

Nenhum código C deve ser gerado quando forem encontrados erros léxicos, sintáticos ou semânticos.

### 13.1 Arrays

Código:

```java
int[] valores = new int[10];
```

Resultado esperado:

```text
ERRO: arrays não são suportados pelo subconjunto Java.
```

### 13.2 String como tipo

Código:

```java
String nome = "Renan";
```

Resultado esperado:

```text
ERRO: o tipo String não é suportado pelo subconjunto Java.
```

### 13.3 Herança

Código:

```java
class Filho extends Pai {
}
```

Resultado esperado:

```text
ERRO: herança não é suportada pelo subconjunto Java.
```

### 13.4 Generics

Código:

```java
List<Integer> valores;
```

Resultado esperado:

```text
ERRO: generics e coleções não são suportados pelo subconjunto Java.
```

### 13.5 Exceções

Código:

```java
try {
    int x = 10;
} catch (Exception e) {
    System.out.println("Erro");
}
```

Resultado esperado:

```text
ERRO: tratamento de exceções não é suportado pelo subconjunto Java.
```

### 13.6 Lambda expressions

Código:

```java
Runnable r = () -> System.out.println("Olá");
```

Resultado esperado:

```text
ERRO: lambda expressions não são suportadas pelo subconjunto Java.
```

### 13.7 Sobrecarga de métodos

Código:

```java
static int soma(int a, int b) {
    return a + b;
}

static double soma(double a, double b) {
    return a + b;
}
```

Resultado esperado:

```text
ERRO: sobrecarga de métodos não é suportada pelo subconjunto Java.
```

---

## 14. Regras gerais de compilação

O compilador deve executar as seguintes etapas:

```text
Código-fonte Java
        ↓
Análise léxica
        ↓
Análise sintática
        ↓
Análise semântica
        ↓
Geração de código C
```

Uma construção somente deve ser traduzida caso seja válida segundo as regras sintáticas e semânticas deste documento.

Caso seja detectado erro em qualquer etapa anterior à geração de código:

```text
Código Java inválido
        ↓
Erro léxico/sintático/semântico
        ↓
Mensagem de erro
        ↓
Nenhum código C gerado
```

O compilador não deve gerar código C parcialmente correspondente a um programa Java inválido.

---

## 15. Resumo de cobertura

| Construção                          | Suportado |
| ----------------------------------- | --------- |
| `int`                               | Sim       |
| `float`                             | Sim       |
| `double`                            | Sim       |
| `char`                              | Sim       |
| `boolean`                           | Sim       |
| `void`                              | Sim       |
| Variáveis locais                    | Sim       |
| Campos `static` / variáveis globais | Sim       |
| Operadores aritméticos              | Sim       |
| Operadores relacionais              | Sim       |
| Operadores lógicos                  | Sim       |
| Operadores de atribuição            | Sim       |
| `++` / `--`                         | Sim       |
| Expressões com precedência          | Sim       |
| `if` / `else if` / `else`           | Sim       |
| `while`                             | Sim       |
| `for`                               | Sim       |
| `do while`                          | Sim       |
| `break` / `continue`                | Sim       |
| Métodos `static`                    | Sim       |
| Parâmetros                          | Sim       |
| `return`                            | Sim       |
| `main`                              | Sim       |
| Impressão simplificada              | Sim       |
| Literais de string em `println`     | Sim       |
| `String` como tipo                  | Não       |
| Arrays                              | Não       |
| Sobrecarga de métodos               | Não       |
| Classes definidas pelo usuário      | Não       |
| Herança                             | Não       |
| Interfaces                          | Não       |
| Classes abstratas                   | Não       |
| Generics                            | Não       |
| Coleções                            | Não       |
| Exceções                            | Não       |
| Threads                             | Não       |
| Reflection                          | Não       |
| Anotações                           | Não       |
| Lambda expressions                  | Não       |
| Streams                             | Não       |
| Bibliotecas externas                | Não       |
| Ponteiros                           | Não       |
| Boxing / unboxing                   | Não       |

---

## 16. Exemplos completos

### 16.1 Programa válido

```java
public class Main {

    static int contador = 0;

    static int soma(int a, int b) {
        return a + b;
    }

    static void incrementar() {
        contador++;
    }

    public static void main(String[] args) {
        int resultado = soma(10, 20);

        if (resultado > 20) {
            System.out.println("Resultado maior que 20");
        } else {
            System.out.println("Resultado menor ou igual a 20");
        }

        for (int i = 0; i < 5; i++) {
            incrementar();
        }

        do {
            contador--;
        } while (contador > 2);

        System.out.println(contador);
    }
}
```

Esse programa utiliza:

* tipos primitivos;
* variável local;
* campo `static`;
* variável global na tradução;
* método `static`;
* parâmetros;
* `return`;
* chamada de método;
* `if`;
* `else`;
* `for`;
* `do while`;
* incremento;
* decremento;
* expressão relacional;
* impressão.

Portanto, representa adequadamente o conjunto de construções necessário para demonstrar o fluxo completo do compilador.

---

## 17. Decisões de escopo

As seguintes decisões são definitivas para a versão atual do compilador:

1. O compilador aceita um subconjunto procedural da linguagem Java.
2. Todos os métodos suportados são `static`.
3. Todos os campos suportados são `static`.
4. Campos `static` são traduzidos para variáveis globais em C.
5. Somente tipos primitivos fazem parte do sistema de tipos.
6. `String` não é suportado como tipo.
7. Literais de string são permitidos exclusivamente na construção simplificada de impressão.
8. Arrays não são suportados.
9. Sobrecarga de métodos não é suportada.
10. `while`, `for` e `do while` são suportados.
11. `break` e `continue` são suportados dentro de estruturas de repetição.
12. Recursos avançados de orientação a objetos não fazem parte do subconjunto.
13. Recursos avançados da biblioteca padrão do Java não fazem parte do subconjunto.
14. Construções fora do escopo devem resultar em erro de compilação.
15. Programas que apresentem erros léxicos, sintáticos ou semânticos não devem gerar código C.

Essas decisões definem o escopo da linguagem Java suportada pelo compilador e fornecem uma especificação suficiente para orientar as etapas de análise léxica, análise sintática, análise semântica e geração de código C.
