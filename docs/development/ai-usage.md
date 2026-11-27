# Declaração de uso de IA

Este projeto registra o uso de inteligência artificial no desenvolvimento do
compilador e na manutenção da documentação. Por se tratar de um tema de difícil
entendimento e envolver programação complexa — analisador léxico em Flex,
gramática Bison, tabela de símbolos, análise semântica, representação
intermediária e geração de C —, a IA foi empregada **de forma geral durante o
projeto**, como apoio à equipe.

A declaração existe para deixar claro o que foi feito com esse apoio e,
sobretudo, o que permanece sob responsabilidade humana.

## Resumo

| Item | Registro |
| --- | --- |
| Ferramentas | Diversos modelos de IA foram usados ao longo do projeto; não há um modelo específico a declarar |
| Abrangência | Uso geral: análise, implementação, testes e documentação |
| Motivo | Tema de difícil entendimento e programação complexa |
| Decisões | **Nenhuma decisão foi tomada pela IA**; todas as decisões são da equipe |
| Revisão | Todas as entregas foram revisadas pela equipe antes de serem aceitas |
| Correções | A equipe fez ajustes inclusive além do escopo original das entregas |
| Referência de verdade | Código-fonte, headers, manifests de teste e saída real do executável |

## Como a IA foi usada

A IA atuou como apoio em tarefas como:

- leitura e análise da estrutura do repositório, do Makefile, da gramática
  Bison, do lexer Flex, dos headers de API e dos manifests de teste;
- elaboração e revisão de código, testes e scripts de verificação;
- escrita e revisão da documentação MkDocs, incluindo as especificações que já
  existiam no repositório —
  [escopo e sintaxe](../definicao-escopo-sintaxe-semantica.md),
  [tokens](../definicao-tokens.md) e
  [tabela de símbolos](../tabela-simbolos.md);
- configuração do site (`mkdocs.yml`, `requirements-docs.txt`) e do workflow de
  publicação no GitHub Actions;
- investigação de divergências entre a documentação antiga e o comportamento
  implementado.

A lista acima descreve a natureza do apoio recebido, não uma autoria isolada de
cada arquivo: o resultado final é sempre uma entrega da equipe.

## Decisões, revisão e correções

- **Nenhuma tomada de decisão foi feita pela IA.** O escopo da linguagem, as
  estruturas de dados, os contratos de API, as escolhas de implementação e as
  prioridades de trabalho foram definidos pelos mantenedores.
- **Todas as entregas foram revisadas.** Cada resultado obtido com apoio de IA
  foi conferido contra o código, os testes e a saída real do compilador antes
  de ser aceito.
- **As correções não se limitaram ao pedido original.** Durante a revisão, a
  equipe realizou ajustes que ultrapassaram o escopo inicial das entregas, para
  manter a consistência do projeto.
- Onde a implementação e documentos antigos divergiam, prevaleceu o
  comportamento implementado; o PDF `especificacao-java-subset.pdf` foi mantido
  apenas como registro histórico.

## Verificação do conteúdo

As afirmações técnicas da documentação não foram aceitas apenas por
plausibilidade. Funções, opções de linha de comando, formatos de saída e alvos
de teste foram conferidos contra:

- `src/main.c` (opções, mensagens e código de saída);
- `parser/ast.h`, `src/symbol_table.h`, `src/ir.h`, `src/ir_gen.h`,
  `src/ir_opt.h`, `src/codegen.h`, `src/semantic_flow.h` e
  `src/const_eval.h` (assinaturas e contratos);
- `parser/parser.y` e `lexer/lexer.l` (gramática, tokens e regras léxicas);
- `Makefile`, `tests/*.sh` e `examples/*/esperado.txt` (comandos e formatos
  esperados);
- execução real de comandos como `./protoparser --ast`, `--tabela`, `--ir` e a
  reprodução de um erro semântico, para confirmar as saídas transcritas.

A validação da documentação incluiu `mkdocs build --strict` (sem avisos),
verificação dos links locais no HTML gerado e um teste de servir o site com
`mkdocs serve`. O comando `make check` não faz parte dessa validação de
documentação e pode ser executado
[conforme o guia de testes](../guides/testing.md) para conferir o compilador.

## Limites e responsabilidades

!!! important "Revisão humana continua necessária"
    Nenhuma entrega obtida com apoio de IA foi aceita sem revisão da equipe. Em
    caso de divergência entre esta documentação e o código ou os testes,
    **o código e os testes prevalecem**. Correções devem ser feitas nas páginas
    afetadas e na especificação correspondente, com `mkdocs build --strict`
    executado antes da publicação.

- A IA não decidiu o escopo da linguagem nem alterou a semântica do compilador
  por conta própria.
- A escolha de modelos e ferramentas variou ao longo do projeto; nenhum modelo é
  apresentado aqui como autor ou responsável pelo conteúdo.
- Esta declaração deve ser atualizada se a forma de uso de IA no projeto
  mudar.

## Leitura relacionada

- [Documentação do projeto](documentation.md) — como escrever e validar páginas.
- [Padrões e convenções](conventions.md) — convenções de código e documentação.
- [Visão geral](../introduction/overview.md) — estado atual do projeto.
