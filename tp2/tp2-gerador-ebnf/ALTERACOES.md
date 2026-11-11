# Alterações feitas no código-base do TP2

Este documento lista **todos** os arquivos criados ou alterados em relação ao código-base fornecido pelo professor e explica cada mudança.

## Como achar as alterações no código

Todo trecho alterado está marcado no próprio fonte:

```c
/* >>>>> TP2 ALTERADO: o que mudou e por quê */
... código novo ou alterado ...
/* <<<<< TP2 FIM */
```

Alterações de uma linha só levam o comentário `/* TP2 ALTERADO */` ao lado. Arquivos que não existiam no código-base têm `[TP2] ARQUIVO NOVO` no cabeçalho. Para listar todas as marcas:

```
grep -rn "TP2" src microc
```

## Resumo

| Arquivo | Situação | Etapa | O que foi feito |
| --- | --- | --- | --- |
| `src/ebnf_parser.c` | alterado | 1. Leitura | `parse_expr`, `parse_seq`, `parse_fator` (+ `inicia_fator`, `parse_grupo`) |
| `src/verificacao.c` | alterado | 2. Semântica | inalcançáveis, improdutivos, recursão à esquerda |
| `src/conjuntos.c` | alterado | 3. Conjuntos | anulável, FIRST, FOLLOW |
| `src/ll1.c` | alterado | 4. LL(1) | as três condições LL(1) |
| `src/codegen.c` | alterado | 5. Geração | `gen_expr`, `gen_casamento_regex`, `gen_token_de_arquivo`, correção do `tmpfile()` no Windows |
| `src/regex.c` | **novo** | 5. Geração | expressão regular → AFN → AFD → código C |
| `src/regex.h` | **novo** | 5. Geração | interface de `regex.c` |
| `src/microc_mapa.c` | alterado | Micro C | `microc_carregar_tokens_h` |
| `microc/tokens.h` | **novo** | Micro C | `TokenType`, `Token`, `YYSTYPE` extraídos do TP1 |
| `microc/microc.flex` | **copiado do TP1 e adaptado** | Micro C | prefixo `microc_yy`, `noyywrap`, `tokens.h`, linha do erro de string, `main` opcional |
| `microc/scanner.c` | alterado | Micro C | lexema da tabela de strings, mensagem de erro, liberação da tabela |
| `testes/gramaticas/*.ebnf` | 5 **novos** | Testes | `json`, `calc`, `sexpr`, `improdutivo`, `indefinido` |
| `testes/esperado/*.gerador` | 5 **novos** | Testes | saída esperada do gerador para as gramáticas novas |
| `testes/programas/{json,calc,sexpr}/` | 21 programas **novos** | Testes | 3 aceitos e 4 rejeitados por gramática, com `.esperado` |
| `ALTERACOES.md` | **novo** | — | este documento |

**Não foram alterados:** `main.c`, `util.c/h`, `bitset.c/h`, `gramatica.c/h`, `ebnf_lexer.c/h`, todos os outros `.h` de `src/`, `microc/scanner.h`, `Makefile`, `testes/executar.sh` e os testes que já vinham no código-base.

---

## Etapa 1: leitura da EBNF (`src/ebnf_parser.c`)

O stub antigo de `parse_expr` descartava o lado direito das regras e devolvia sempre `E_VAZIO`. Ele foi trocado por um analisador descendente recursivo, com uma função por não terminal da notação:

```
expr  = seq { "|" seq } ;
seq   = { fator } ;
fator = NOME_TERM | LITERAL | NAOTERM | "(" expr ")" | "[" expr "]" | "{" expr "}" ;
```

- **`inicia_fator`**: diz se o token corrente pode começar um fator (é o FIRST de `fator`); é ela que decide onde uma sequência termina.
- **`parse_grupo`**: trata `( )`, `[ ]` e `{ }`: consome a abertura, lê a expressão interna e exige o fechamento. `( a )` não cria nó; `[ a ]` vira `E_OPC` e `{ a }` vira `E_REP`.
- **`parse_fator`**: terminal (`E_TERM`), não terminal (`E_NAOTERM`) ou grupo.
- **`parse_seq`**: zero fatores vira `E_VAZIO`; um fator é devolvido direto; mais de um vira `E_SEQ`.
- **`parse_expr`**: uma alternativa é devolvida direto; mais de uma vira `E_ALT`.

Em caso de erro, a árvore parcial é devolvida e `parse_regra` (já fornecida) percebe o erro pelo contador de diagnósticos e a descarta.

## Etapa 2: verificação semântica (`src/verificacao.c`)

Os três stubs retornavam 0.

- **`verif_inalcancaveis`** (aviso): marca o símbolo inicial e, até nada mudar, marca todo não terminal usado na regra de um já marcado (via `expr_percorrer` + `marcar_usados`). Os que sobram geram `aviso: nao terminal X inalcancavel a partir do simbolo inicial S`.
- **`verif_improdutivos`** (erro): ponto fixo com `expr_produtiva`. Terminal, vazio, `[ ]` e `{ }` são produtivos; uma sequência exige todos os filhos produtivos e uma alternativa, ao menos um.
- **`verif_recursao_esquerda`** (erro):
  - `canto_esquerdo` monta a matriz "B pode começar a regra de A". Na sequência, olha os filhos até o primeiro não anulável, por isso `a = [ x ] a` também conta.
  - Depois vem o fecho transitivo (algoritmo de Warshall).
  - Se A começa a própria regra, a recursão é **direta**; se só volta a si passando por outros não terminais, é **indireta**.

## Etapa 3: anulável, FIRST e FOLLOW (`src/conjuntos.c`)

Os três stubs eram vazios. Todos seguem o mesmo padrão: uma função recursiva visita a árvore `Expr` de cada regra e devolve 1 se algo mudou; a função pública repete as passadas até um ponto fixo.

- **`anulavel_expr`**: calcula primeiro os filhos e depois o nó.
- **`first_expr`**: calcula o FIRST de **todos** os filhos de uma sequência, inclusive dos que não entram no FIRST dela, porque o gerador usa o FIRST de cada filho. O FIRST da sequência junta os filhos até o primeiro não anulável.
- **`follow_expr`**: desce da regra para os filhos.
  - Na sequência, anda da direita para a esquerda guardando o "resto": FIRST do sufixo, mais o follow da sequência enquanto o sufixo for anulável.
  - Em `{ a }`, o follow de `a` também recebe FIRST(a).
  - FOLLOW(inicial) começa com `$`.

## Etapa 4: condição LL(1) (`src/ll1.c`)

O stub retornava 0.

- **`verificar_alt`** (em `a1 | ... | an`):
  - (1a) FIRST disjuntos dois a dois;
  - (1b) no máximo uma alternativa anulável;
  - (2) se uma alternativa é anulável, o FOLLOW dela não pode cruzar o FIRST das outras.

  Todas são **erros**.
- **`verificar_opc_rep`** (em `[ a ]` e `{ a }`):
  - `a` anulável é **erro**;
  - FIRST(a) ∩ FOLLOW ≠ ∅ é só **aviso**, resolvido de forma gulosa. É o caso do `else` pendente do Micro C.
- **`verificar_expr`** percorre a árvore e **`ll1_verificar`** soma os erros. Avisos não contam como erros.

## Etapa 5: geração de código (`src/codegen.c`, `src/regex.c`, `src/regex.h`)

### `gen_expr` (`codegen.c`)

Antes, só terminal e não terminal eram traduzidos; o resto virava um comentário `TODO`. Agora segue o esquema da seção 4 do enunciado:

| EBNF | Código gerado |
| --- | --- |
| `a1 a2 ... an` | tradução de cada `ai`, em ordem |
| `a1 \| ... \| an` | `if (FIRST(a1)) {...} else if ... else erro_sintatico(FIRST(todo o \|))` |
| `[ a ]` | `if (FIRST(a)) {...}` |
| `{ a }` | `while (FIRST(a)) {...}` |
| vazio | nada |

A alternativa anulável (no máximo uma, garantido pela etapa 4) fica sem teste, no `else` final, no lugar do erro.

### `gen_casamento_regex` (`codegen.c`) + `regex.c` (novo)

O stub gerava uma função que sempre devolvia 0, ou seja, nenhum token era reconhecido. Agora `gen_casamento_regex` chama `regex_gerar_c`, do módulo novo `regex.c`, que funciona em três passos:

1. **Análise da regex** (descendente recursiva): `alt = concat { "|" concat }`, `concat = { repet }`, `repet = atomo { * | + | ? }`, `atomo = ( alt ) | [classe] | . | \c | c`. Cada função monta um fragmento de **AFN de Thompson**.
2. **Construção de subconjuntos**: cada estado do **AFD** é um conjunto de estados do AFN fechado por transições vazias. Há um limite de 4000 estados como proteção.
3. **Emissão**: uma função `static size_t casa_X(const char *s)` com um `switch` sobre o estado do AFD. As transições são agrupadas em faixas (`c >= 'a' && c <= 'z'`) e o código guarda a última posição em que passou por um estado final, devolvendo assim o **maior prefixo** reconhecido.

A sintaxe aceita é concatenação, `|`, `*`, `+`, `?`, parênteses, `[a-z]`, `[^...]`, `.` (qualquer caractere exceto `\n`) e escapes (`\n`, `\t`, `\r`, `\\`, `\/`, `\.` etc.). Uma regex inválida gera erro com o motivo, por exemplo `expressao regular /a(b/ invalida: falta ')'`.

### `gen_token_de_arquivo` (`codegen.c`)

O stub só imprimia "opcao --tokens ainda nao implementada". Agora o analisador gerado lê, com `fgets` + `sscanf`, linhas no formato:

```
Token: tipo = <numero>, lexema = '<texto>', linha = <numero>
```

- Linhas em branco são ignoradas.
- O lexema vai da primeira `lexema = '` até a **última** `', linha =`, por isso aspas e vírgulas dentro dele funcionam.
- Uma linha malformada gera `arquivo de tokens, linha N: linha malformada`.
- No fim do arquivo, o analisador devolve `TOKEN_FIM`.

### Correção do `tmpfile()` no Windows (`codegen.c`, `gerar_c`)

No Windows, o `tmpfile()` do msvcrt tenta criar o arquivo na raiz do disco (`C:\`) e falha sem permissão de administrador. Por isso o gerador não conseguia escrever o `.c`. Agora, se `tmpfile()` falhar, ele usa `prefixo.tmp` e o apaga no fim. **No Linux o comportamento é o mesmo de antes.**

## Integração com o scanner do TP1 (Micro C)

O scanner veio do repositório `trab_compiladores`, branch **`vini`**, a única com o `microc.flex` completo (sem `TODO`).

### `microc/tokens.h` (novo)

No TP1, o `enum TokenType` ficava dentro do `.flex`. Ele foi movido para este cabeçalho, como pede a seção 3.1, junto com:
- `Token` (`tipo`, `lexema[100]`, `linha`, como no livro);
- `YYSTYPE` e a declaração `extern` de `microc_yylval`;
- o protótipo de `microc_tabela_liberar()`.

A ordem do `enum` é a do livro (`PRINT` = 36).

### `microc/microc.flex` (copiado do TP1 e adaptado)

- `%option prefix="microc_yy" noyywrap` acrescentado e `yywrap()` removida.
- O `typedef enum TokenType` e o `typedef YYSTYPE` foram trocados por `#include "tokens.h"`.
- **"String nao terminada"**: antes, a regra fazia `linha_atual++` antes de devolver o erro, e o erro saía na linha seguinte. Agora usa `yyless(0)` e a quebra de linha é contada depois, então o erro sai na linha da string. É o que o teste `rejeitado_lexico.mc` espera.
- `main()` de teste e `nome_token[]` só são compilados com `-DMICROC_LEXER_MAIN`, para não colidir com o `main()` do analisador gerado.
- Nova função `microc_tabela_liberar()`, que libera a tabela de strings e reinicia o estado.

O resto do scanner (regras, tabela de strings, escapes, inteiros negativos) é o do TP1, sem mudanças.

### `microc/scanner.c`

- `texto_do_lexema`: para `ID` e constantes, devolve `microc_yylval.symbol` (texto da tabela de strings, já com escapes convertidos). Para `UNDEF`, devolve `microc_yylval.error_msg`. Antes devolvia sempre `microc_yytext`.
- `scanner_finalizar`: chama `microc_tabela_liberar()` antes de `microc_yylex_destroy()`.

### `src/microc_mapa.c`: `microc_carregar_tokens_h`

O stub só emitia um aviso e pulava a verificação. Agora a função:
- lê o `tokens.h` indicado em `TOKENS EXTERN`;
- remove os comentários (`sem_comentarios`);
- acha a enumeração `enum { ... } TokenType` (`extrair_enum`);
- registra cada nome, ignorando valores explícitos (`X = 3`).

Com isso, um terminal que não existe no `TokenType` (por exemplo `FOO`) passa a dar erro.

## Testes novos

O enunciado pede pelo menos 5 gramáticas e, para cada gramática válida, 3 programas aceitos e 3 rejeitados. Agora são **9 gramáticas**:

| Gramática | Válida? | O que testa |
| --- | --- | --- |
| `expr` (já existia) | sim | expressões aritméticas |
| `microc` (já existia) | sim, com aviso | Micro C com scanner do TP1, `else` pendente |
| `conflito_ll1` (já existia) | não | conflito FIRST/FIRST |
| `recursao_esquerda` (já existia) | não | recursão à esquerda direta e indireta |
| **`json`** | sim | regex com classe negada e escapes, opcionais aninhados em `NUM`, listas `[ x { "," x } ]` |
| **`calc`** | sim | dois `SKIP` (brancos e comentário `--`), palavra reservada × ID, `=` × `==`, `-` unário, `if`/`else`/`while` |
| **`sexpr`** | sim | símbolo inicial anulável, `\/` dentro de classe, desempate entre `NUM` e `ATOM` |
| **`improdutivo`** | não | não terminal improdutivo (erro) e inalcançável (aviso) |
| **`indefinido`** | não | não terminal indefinido e terminal não declarado |

Programas novos (cada um com seu `.esperado`, conferido à mão):

| Pasta | Aceitos | Rejeitados |
| --- | --- | --- |
| `json/` | objeto completo, `[]`, aninhado com escapes | `,` sobrando, `]` faltando (fim de arquivo), `tru` (erro léxico), `:` faltando |
| `calc/` | laço `while`, programa só com comentários, `if`/`else` + `iffy` como ID | `end` faltando, `x = ;`, `#` (erro léxico), `else` fora do `if` |
| `sexpr/` | fatorial, `'(1 -5 a/b ())`, arquivo vazio | `)` sobrando, `)` faltando, `"` (erro léxico), `'` sem operando |

**Resultado:** 43 de 43 testes passam: 9 gramáticas + 34 programas (`expr` 7, `microc` 6, `json` 7, `calc` 7, `sexpr` 7).

## Como compilar e testar

No Linux ou WSL, que é o ambiente da correção:

```
make
make microc
make test
```

No Windows (Git Bash + MSYS2), sem `make`:

```
export PATH="/c/msys64/mingw64/bin:$PATH"
gcc -ansi -pedantic -Wall -Wextra -g -o gerador src/*.c
mkdir -p build/microc
./gerador testes/gramaticas/microc.ebnf -o build/microc/parser --scanner microc --tree
/c/msys64/usr/bin/flex -o build/microc/lex.yy.c microc/microc.flex
gcc -g -w -Imicroc -c -o build/microc/lex.yy.o build/microc/lex.yy.c
gcc -ansi -pedantic -Wall -Wextra -Imicroc -Ibuild/microc -o build/microc/parser_microc build/microc/parser.c microc/scanner.c build/microc/lex.yy.o
sed 's/diff -u /diff -u --strip-trailing-cr /' testes/executar.sh > /tmp/executar_win.sh && bash /tmp/executar_win.sh
```

Cuidados no Windows:
- **Não** ponha `C:\msys64\usr\bin` na frente do PATH. Misturado com o Git Bash, o `sh` do MSYS2 perde a variável `TMP`, e o gcc falha com "Cannot create temporary file in C:\WINDOWS\". Chame o `flex` pelo caminho completo.
- O gerador escreve `\r\n` no Windows, e os `.esperado` usam `\n`. Por isso o `diff` precisa de `--strip-trailing-cr`. No Linux isso não acontece.
