# Como ler `RESULTADOS.txt`

Este arquivo contém a saída real do `protoparser` para cada um dos 43 casos
de teste léxico (`L01.java` a `L43.java`), gerada rodando de fato:

```bash
./protoparser examples/lexico/LNN.java
```

## Importante: léxico vs. sintático

O protótipo atual não expõe o analisador léxico isoladamente — ele já vem
acoplado ao parser Bison. Isso significa que:

- Casos que testam **apenas um token ou fragmento solto** (ex.: `L01.java`
  contém só `while`, `L06.java` contém só `2x`) são **lexicamente
  corretos**, mas o parser acusa `ERRO DE SINTAXE`, porque a gramática
  espera uma instrução completa (declaração, atribuição, `if`, `while`
  etc.), não um token avulso.
  → Nesses casos, o que importa observar é se **não aparece** a mensagem
  `ERRO LEXICO`. Se só aparecer `ERRO DE SINTAXE`, o lexer fez o trabalho
  certo (reconheceu o token) e quem "reclamou" foi o parser, como esperado.

- Casos com `int x = 10 @ 5;` (`L34`), string não terminada (`L21`), char
  malformado (`L17`–`L19`), comentário de bloco sem fechar (`L26`), etc.
  devem mostrar a mensagem `ERRO LEXICO na linha N: ...` — esse sim é o
  sinal de que o lexer está funcionando corretamente.

- Casos com instrução completa e válida (`L07`, `L08`, `L14`, `L20`, `L23`,
  `L24`, `L25`, `L28`–`L31`, `L39`, `L40`, `L41`) devem terminar com
  `Analise concluida sem erros` e `exit code: 0`.

## Resumo rápido por resultado esperado

| Resultado no `RESULTADOS.txt` | Significa |
|---|---|
| `ERRO LEXICO na linha N` presente | Lexer rejeitou corretamente um caractere/lexema inválido |
| Só `ERRO DE SINTAXE`, sem `ERRO LEXICO` | Lexer aceitou os tokens; quem barrou foi a gramática (normal para fragmentos soltos como L01–L06, L09, L32, L37, L38, L42, L43) |
| `Analise concluida sem erros` | Caso totalmente válido, léxico e sintático |

Para isolar só o comportamento léxico sem depender do parser, uma extensão
futura recomendada é compilar `lexer.l` sozinho (com um `main()` mínimo que
só chama `yylex()` em loop e imprime cada token), sem ligar ao Bison. Isso
não foi feito neste protótipo porque o objetivo da issue original era
validar a integração Flex+Bison, não o lexer isolado.
