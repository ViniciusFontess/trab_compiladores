# Relatório — TP1 Micro C (versão de estudo, formato simples)

**O que é esta pasta:** uma reescrita do TP1 de Análise Léxica (Micro C),
feita do zero, no mesmo estilo simples e direto do seu
`analisador_lexico` — um único `microc.flex`, comentários explicando o
"porquê" de cada regra, `tests/` e `tests/saidas/`. O objetivo original
era te dar uma versão que desse para ler de cima a baixo e entender por
completo; depois de uma conversa longa explicando cada parte do código,
a pasta foi organizada no formato de entrega (`leiame.txt`, `USO_IA.md`)
para servir de base real, se o grupo decidir usá-la.

**Relação com a versão em `origin/vini`:** esta pasta é uma versão
**paralela**, não uma substituição — ela não foi mesclada com o que já
existe na branch do grupo. Veja `USO_IA.md` para o registro completo do
uso de IA nesta versão, e `leiame.txt` para os dados de entrega.

## 1. O que mudou em relação ao `analisador_lexico`

A ideia central é a mesma dos dois trabalhos anteriores (o seu
`analisador_lexico` e o `aula8-lexico` do grupo): reconhecer o maior
lexema possível (maximal munch), tratar palavra reservada depois do
padrão de identificador, e usar `TK_EOF`/`END_OF_FILE` como sentinela.
O que o TP1 acrescenta, e que exigiu ideias novas:

**a) O scanner devolve o token, não imprime direto.**
No `analisador_lexico`, cada regra chamava `imprime_token()` na hora.
Aqui, cada regra faz `return TIPO;` — é o `main()` (ou, no futuro, o
analisador sintático) que decide o que fazer com o token. Isso é
exigido porque o próximo trabalho prático vai *chamar* esse scanner
regra por regra, pedindo "me dê o próximo token", em vez de deixar ele
rodar sozinho até o fim imprimindo tudo.

**b) Comentários de bloco e strings precisam de um "modo" à parte
(`%x`).**
Um comentário `//` cabe inteiro numa expressão regular (`"//".*`,
porque termina na quebra de linha). Já um comentário `/* ... */` pode
ter várias linhas e qualquer conteúdo no meio — não dá pra escrever uma
única expressão regular pra isso. A saída é o Flex ter "modos"
exclusivos (`%x COMENTARIO`, `%x STRING`): ao ver `/*`, entramos no modo
`COMENTARIO` (`BEGIN(COMENTARIO)`) e todas as regras seguintes com o
prefixo `<COMENTARIO>` passam a valer, até acharmos `*/` (ou o arquivo
acabar, que é erro). O mesmo vale para `STRING`. Fora desses modos,
essas regras nem existem para o Flex.

**c) Erro léxico não é impresso na hora — vira um token `UNDEF` com uma
mensagem guardada.**
Diferente do `analisador_lexico` (onde o erro ia direto pro `stderr`
dentro da própria regra), aqui a regra só registra a mensagem em
`valor_atual.erro` e devolve `UNDEF`. Quem decide o que fazer com esse
erro é o `main()` — porque, de novo, no trabalho seguinte quem vai
decidir isso é o parser, não o scanner.

**d) Número negativo: o scanner precisa "lembrar" do token anterior.**
`5 - 2` é subtração; `x = -2;` é um inteiro negativo. Os dois têm
exatamente o mesmo `-` na entrada. A única forma de diferenciar é
olhando o que veio ANTES do `-`: se foi algo que pode "terminar" uma
expressão (um identificador, um número, um `)`, um `]`), o `-` é
operador; senão, é sinal. Por isso existe a variável `ultimo_token`,
atualizada no `main()` depois de cada chamada ao scanner, e a função
`pode_terminar_expressao()`. Quando decidimos que o `-` era operador
(mas o Flex já tinha casado `-` junto com os dígitos seguintes, por ser
o casamento mais longo), usamos `yyless(1)` para "devolver" os dígitos
pro Flex reler, ficando só o `-` como token atual.

## 2. Erros léxicos tratados (Seção 4.1 do enunciado)

| Situação | Mensagem | Onde no `microc.flex` |
|---|---|---|
| Caractere fora do alfabeto (`@`, `#`...) | o próprio caractere vira o lexema de erro | regra coringa `.` no final |
| `*/` sem `/*` antes | `Comentario nao iniciado` | regra `"*/"` fora do modo `COMENTARIO` |
| `/* ...` até o fim do arquivo sem fechar | `EOF em comentario` | `<COMENTARIO><<EOF>>` |
| String com quebra de linha antes do fechamento | `String nao terminada` | `<STRING>\n` |
| String até o fim do arquivo sem fechar | `EOF em string` | `<STRING><<EOF>>` |
| Caractere nulo dentro de uma string | `String contem caractere nulo` | `<STRING>\0` |
| `'ab'` (mais de um caractere) | `Constante de caractere invalida` | regra `"'"[^'\n]*"'"` |
| `'a` sem fechar | `Constante de caractere nao terminada` | regra `"'"[^'\n]*` |

Um detalhe interessante do teste `tests/char_constantes.mc`: na linha `e2 = 'z;`,
a regra de "não terminada" é gulosa (`[^'\n]*`) e acaba engolindo o
`;` junto com o erro — o lexema de erro vira `'z;` inteiro, e não sobra
um `SEMICOLON` separado nessa linha. Não é bug: é a expressão regular
fazendo exatamente o que foi escrita para fazer (consumir tudo até a
quebra de linha). Vale notar isso na hora de explicar o código.

**Caractere nulo dentro de string — por que não tem teste em
`tests/`:** teria que existir um byte `\0` de verdade no meio do
arquivo `.mc`, o que não dá pra escrever como texto simples. A regra
`<STRING>\0` está implementada e é direta de ler; se quiser testar de
verdade, o arquivo `trab_compiladores/tests/erro_nulo.mc` (na branch
`origin/vini`) já tem esse caso pronto.

## 3. Casos de teste

| Arquivo | O que exercita |
|---|---|
| `tests/test.mc` | programa completo: reservadas, id, número, operador, pontuação |
| `tests/comentario_bloco.mc` | comentário `//` e comentário `/* */` de várias linhas |
| `tests/string_valida.mc` | string com `\"` e `\\` (escolhido para não ter caractere de controle "quebrando" a linha da saída) |
| `tests/erro_string_nao_terminada.mc` | string cortada por quebra de linha |
| `tests/erro_eof_string.mc` | string cortada pelo fim do arquivo (sem `\n`) |
| `tests/char_constantes.mc` | char válido, char com escape (`\t`), char inválido, char não terminado |
| `tests/inteiro_negativo.mc` | subtração vs. negativo depois de `=`, `(` e depois de outro `-` |
| `tests/operadores.mc` | todos os operadores de dois caracteres, `[`/`]`, `%`, `,` |
| `tests/erro_comentario_diversos.mc` | caractere inválido, `*/` sem abertura, `EOF em comentario` |

As saídas em `tests/saidas/` foram **rastreadas manualmente** regra por
regra — não há `flex`/`gcc` disponíveis neste ambiente (nem no WSL desta
máquina) para compilar e conferir de verdade. Antes de confiar nelas
100%, rode você mesmo, do mesmo jeito que fez no `analisador_lexico`:

```
flex microc.flex
gcc lex.yy.c -o lexer -lfl
./lexer tests/test.mc
```

Se alguma saída não bater com o que está em `tests/saidas/`, o erro
provavelmente está no rastreamento manual, não necessariamente no
`microc.flex` — vale a pena investigar os dois lados.

## 4. Simplificações conscientes (em relação à versão de `origin/vini`)

- **Tabela de strings com tamanho fixo** (`TABELA_MAX 1024`) em vez de
  crescer dinamicamente com `realloc`. Mais simples de ler; para os
  programas de teste do TP1 nunca vai chegar perto do limite.
- **Sem verificação de "string longa demais"** — o buffer da string
  (`STR_MAX`) trunca em silêncio se estourar. O enunciado não pede esse
  caso, então não criei uma mensagem de erro para ele.
- **Sem `yylex()` renomeado.** A versão de `origin/vini` usa uma macro
  (`YY_DECL`) pra renomear a função gerada pelo Flex e conseguir
  "interceptar" o retorno antes de devolver ao chamador. Aqui,
  `ultimo_token` é atualizado direto no laço do `main()`, depois de cada
  chamada — mesmo efeito, uma camada a menos para entender.

Essas escolhas trocam um pouco de generalidade por clareza — o
suficiente pra você acompanhar o código inteiro e depois olhar a versão
mais elaborada do grupo sabendo o que cada peça faz.
