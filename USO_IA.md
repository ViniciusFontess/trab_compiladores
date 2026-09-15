# USO_IA.md

## Entrada 1
- **Ferramenta:** Claude Code (Claude Opus 5 / Sonnet 5, em turnos diferentes da mesma conversa)
- **Trecho:** `microc.flex` (linhas 123-127 — regra de palavras reservadas)
- **Finalidade:** Esclarecer dúvidas conceituais sobre `strcmp`, `strdup`, o uso da função `tabela_guarda()` para gerenciamento da tabela de strings/lexemas e o preenchimento dos campos da struct `ValorToken`. Entender o papel da regra do *maximal munch* no Flex antes da verificação da tabela de reservadas.
- **O que fiz:** Utilizei a explicação conceitual sobre reaproveitamento de ponteiros com `strdup`/`tabela_guarda()` para entender o fluxo do lexema e escrevi a implementação das regras no arquivo `microc.flex`.

## Entrada 2
- **Ferramenta:** Claude Code (Claude Opus 5 / Sonnet 5, em turnos diferentes da mesma conversa)
- **Trecho:** `microc.flex` (linhas 151-154 — regra do inteiro negativo vs. subtração)
- **Finalidade:** Compreender o funcionamento de `yyless(1)` para devolver caracteres ao buffer do Flex e o papel das variáveis de contexto (`ultimo_token` e `pode_terminar_expressao`) para diferenciar a operação de subtração de um literal inteiro negativo.
- **O que fiz:** Compreendi a lógica de gerenciamento de buffer do Flex e adaptei a estrutura condicional para acionar o `yyless` e atualizar as variáveis de estado de acordo com os requisitos do trabalho.

## Entrada 3
- **Ferramenta:** Claude Code (Claude Opus 5 / Sonnet 5, em turnos diferentes da mesma conversa)
- **Trecho:** `microc.flex` (linhas 164-167 — regra de `CHARCONST`)
- **Finalidade:** Entender o tratamento dos caracteres de escape via `converte_escape()` e a utilização da cláusula `default` no `switch` em C para simplificar a devolução de caracteres literais (como `\"` e `\\`).
- **O que fiz:** Apliquei a sugestão conceitual do `default` para reduzir a verbosidade do tratamento de escapes na regra do `CHARCONST` e tratar adequadamente as constantes de caractere no scanner.

## Entrada 4
- **Ferramenta:** Claude Code (Claude Opus 5 / Sonnet 5, em turnos diferentes da mesma conversa)
- **Trecho:** `microc.flex` (linhas 184-187 — regra de `STRINGCONST`)
- **Finalidade:** Esclarecer o acúmulo e conversão de sequências de escape nos buffers auxiliares `str_buf` e `str_add`, compreendendo a diferença entre o texto convertido final e o texto bruto retornado pelo Flex em `yytext`.
- **O que fiz:** Implementei a rotina de montagem de strings utilizando o buffer dedicado (`str_buf`), garantindo que a string armazenada contenha os caracteres de escape já interpretados/convertidos.

## Entrada 5
- **Ferramenta:** Claude Code (Claude Opus 5 / Sonnet 5, em turnos diferentes da mesma conversa)
- **Trecho:** `microc.flex` (linhas 221-224 — operadores restantes com prefixo compartilhado)
- **Finalidade:** Entender o comportamento padrão do Flex em relação ao *maximal munch* para operadores com prefixos comuns (ex: `==` e `=`), confirmando o contraste em que regras distintas dispensam a lógica de estado usada na regra do hífen/número negativo.
- **O que fiz:** Com base no esclarecimento de que o Flex prioriza o casamento mais longo nativamente, escrevi diretamente as regras simples para os operadores restantes sem adicionar verificações desnecessárias.