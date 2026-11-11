/*
 * microc.flex
 *
 * Analisador lexico (scanner) para a linguagem Micro C.
 * Disciplina: Compiladores I - FACOM
 *
 * [TP2] ARQUIVO COPIADO DO TP1 (repositorio trab_compiladores, branch
 * vini, a unica com o scanner completo) e ADAPTADO para o TP2 (secao 3.1
 * do enunciado). Cada adaptacao esta marcada com "TP2 ALTERADO":
 *   - prefixo microc_yy (o flex gera microc_yylex, microc_yytext...);
 *   - TokenType, Token e YYSTYPE movidos para tokens.h;
 *   - "String nao terminada" reportada na linha da string (yyless);
 *   - main() de teste so e compilado com -DMICROC_LEXER_MAIN;
 *   - microc_tabela_liberar() libera a tabela de strings;
 *   - yywrap() removida (substituida por %option noyywrap).
 *
 * Compilacao isolada (lexer de teste do TP1):
 *      flex microc.flex
 *      gcc -DMICROC_LEXER_MAIN lex.yy.c -o lexer
 *
 * Uso:
 *      ./lexer test.mc
 */

/* TP2 ALTERADO: linha %option acrescentada (prefixo exigido pelo TP2) */
%option prefix="microc_yy" noyywrap

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* >>>>> TP2 ALTERADO: o typedef enum TokenType, o vetor nome_token e o
 * typedef YYSTYPE que ficavam aqui foram movidos para tokens.h (o
 * nome_token foi para perto do main de teste, no fim do arquivo). */
/* ---------------------------------------------------------------------
 * 1. VOCABULARIO DE TOKENS: TokenType, Token e YYSTYPE em tokens.h
 * ------------------------------------------------------------------- */
#include "tokens.h"
/* <<<<< TP2 FIM */

YYSTYPE microc_yylval;

/* Linha atual do arquivo-fonte sendo processado. Deve ser incrementada
 * toda vez que uma quebra de linha for consumida pelo scanner (seja em
 * codigo "normal", dentro de comentarios ou dentro de strings). */
int linha_atual = 1;

/* ---------------------------------------------------------------------
 * Tabela de strings (Secao 4.2 do enunciado)
 *
 * Guarda cada lexema distinto uma unica vez. Quando o mesmo identificador
 * (ou constante) aparece de novo, devolvemos o ponteiro ja armazenado em
 * vez de fazer uma nova copia. Implementada como um vetor dinamico com
 * busca linear: simples de explicar e suficiente para programas pequenos.
 * ponytail: busca linear O(n); trocar por hash se o numero de lexemas
 * distintos passar de alguns milhares.
 * ------------------------------------------------------------------- */
static char **tabela_strings = NULL;
static int    tabela_tam = 0;   /* entradas em uso     */
static int    tabela_cap = 0;   /* capacidade do vetor */

static char *tabela_add(const char *s) {
    for (int i = 0; i < tabela_tam; i++)
        if (strcmp(tabela_strings[i], s) == 0)
            return tabela_strings[i];          /* ja existe: reaproveita */

    if (tabela_tam == tabela_cap) {
        tabela_cap = tabela_cap ? tabela_cap * 2 : 64;
        tabela_strings = realloc(tabela_strings, tabela_cap * sizeof *tabela_strings);
        if (!tabela_strings) { fprintf(stderr, "Erro: sem memoria\n"); exit(1); }
    }
    tabela_strings[tabela_tam] = strdup(s);
    if (!tabela_strings[tabela_tam]) { fprintf(stderr, "Erro: sem memoria\n"); exit(1); }
    return tabela_strings[tabela_tam++];
}

/* Funcao auxiliar para preencher microc_yylval.symbol com o texto
 * reconhecido (yytext), passando pela tabela de strings. */
static void guarda_lexema(void) {
    microc_yylval.symbol = tabela_add(yytext);
}

/* Palavras reservadas: comparadas com strcmp() contra o lexema casado
 * pela regra de identificador. Em tabela para nao repetir oito if/else. */
static const struct { const char *texto; TokenType tipo; } reservadas[] = {
    { "main", MAIN }, { "if",     IF     }, { "else", ELSE }, { "for",   FOR   },
    { "int",  INT  }, { "return", RETURN }, { "char", CHAR }, { "print", PRINT },
};
#define N_RESERVADAS (sizeof reservadas / sizeof reservadas[0])

/* Converte uma sequencia de escape (o caractere apos a barra) no
 * caractere correspondente. Escapes desconhecidos devolvem o proprio
 * caractere (ex.: "\q" vira 'q'). */
static char converte_escape(char c) {
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case '0':  return '\0';
        default:   return c;   /* cobre \\, \", \' e os demais */
    }
}

/* Buffer usado para montar o valor de uma STRINGCONST enquanto o scanner
 * esta no estado STRING (o valor convertido nao e o yytext bruto).
 * ponytail: tamanho fixo; strings maiores viram erro lexico. */
#define STR_MAX 4096
static char str_buf[STR_MAX];
static int  str_len;
static int  str_com_nulo;   /* viu \0 literal dentro da string? */
static int  str_estourou;   /* passou de STR_MAX?               */

static void str_add(char c) {
    if (str_len < STR_MAX - 1) str_buf[str_len++] = c;
    else str_estourou = 1;
}

/* Ultimo token devolvido ao chamador. Usado para decidir se um '-' seguido
 * de digitos e um inteiro negativo ou o operador de subtracao: se o token
 * anterior pode terminar um operando (x, 10, 'c', ")" ou "]"), o '-' e
 * subtracao; caso contrario faz parte do numero. */
static int ultimo_token = -1;

static int termina_operando(int t) {
    return t == ID || t == INTEGERCONST || t == CHARCONST || t == STRINGCONST
        || t == RPAREN || t == RBRACKET;
}

/* Renomeia a funcao gerada pelo flex para que yylex() (definida na secao
 * 4) possa registrar o ultimo token antes de devolve-lo. */
#define YY_DECL static int lex_interno(void)

%}

/* -----------------------------------------------------------------------
 * 2. SECAO DE DEFINICOES
 * ------------------------------------------------------------------- */

DIGIT       [0-9]
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]

%x COMMENT
%x STRING

%%

 /* -----------------------------------------------------------------------
  * 3. SECAO DE REGRAS
  * --------------------------------------------------------------------- */

 /* --- Fim de arquivo -----------------------------------------------------
  * Tratada explicitamente (em vez de depender do retorno automatico 0
  * do flex), pois o token UNDEF tambem vale 0 no enum TokenType -- se
  * dependessemos do comportamento padrao, um erro lexico seria
  * confundido com o fim do arquivo pelo main() de teste abaixo.
  * Restrita a INITIAL: os estados COMMENT e STRING tem suas proprias
  * regras de EOF (que sao erros). */
<INITIAL><<EOF>>    { return END_OF_FILE; }

 /* --- Espacos em branco e quebras de linha ---------------------------- */
\n                  { linha_atual++; }
[ \t\r]+            { /* ignora espacos em branco */ }

 /* --- Comentarios ------------------------------------------------------ */
"//".*              { /* comentario de linha: ignora ate o fim da linha */ }

"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         { linha_atual++; }
<COMMENT><<EOF>>    {
                        BEGIN(INITIAL);
                        microc_yylval.error_msg = "EOF em comentario";
                        return UNDEF;
                    }
<COMMENT>.          { /* consome qualquer outro caractere dentro do comentario */ }

 /* Fechamento de comentario sem abertura correspondente. */
"*/"                {
                        microc_yylval.error_msg = "Comentario nao iniciado";
                        return UNDEF;
                    }

 /* --- Palavras reservadas e identificadores ----------------------------
  * Toda sequencia de letras/underscore casa aqui; o lexema e comparado
  * com cada palavra reservada e, se nao for nenhuma, e um ID. */
{LETRA}{ALFANUM}*   {
                        for (size_t i = 0; i < N_RESERVADAS; i++)
                            if (strcmp(yytext, reservadas[i].texto) == 0)
                                return reservadas[i].tipo;
                        guarda_lexema();
                        return ID;
                    }

 /* --- Constantes inteiras -----------------------------------------------
  * Um ou mais digitos, opcionalmente precedidos de '-'. A segunda regra
  * casa "-123" inteiro (mais longo que o "-" sozinho) e so entao decide,
  * pelo token anterior, se o '-' era de fato parte do numero. */
{DIGIT}+            {
                        guarda_lexema();
                        return INTEGERCONST;
                    }

"-"{DIGIT}+         {
                        if (termina_operando(ultimo_token)) {
                            /* "x-1": o '-' e subtracao. Devolve os digitos
                             * para a entrada e casa apenas o '-'. */
                            yyless(1);
                            return MINUS;
                        }
                        /* "= -1", "(-1", "-1": inteiro negativo. */
                        guarda_lexema();
                        return INTEGERCONST;
                    }

 /* --- Constantes de caractere --------------------------------------------
  * Um caractere comum ou uma sequencia de escape entre aspas simples.
  * O valor armazenado e o caractere ja convertido (ex.: '\n' vira a
  * quebra de linha real). As regras seguintes casam as formas invalidas:
  * vazia ('') ou com mais de um caractere ('ab'), e sem fechamento. */
"'"([^'\\\n]|\\.)"'" {
                        char valor[2];
                        valor[0] = (yytext[1] == '\\') ? converte_escape(yytext[2]) : yytext[1];
                        valor[1] = '\0';
                        microc_yylval.symbol = tabela_add(valor);
                        return CHARCONST;
                    }
"'"[^'\n]*"'"       {
                        microc_yylval.error_msg = "Constante de caractere invalida";
                        return UNDEF;
                    }
"'"[^'\n]*          {
                        microc_yylval.error_msg = "Constante de caractere nao terminada";
                        return UNDEF;
                    }

 /* --- Constantes de string -------------------------------------------
  * A string e lida caractere a caractere dentro do estado STRING, com o
  * valor convertido sendo acumulado em str_buf. Isso permite tratar as
  * sequencias de escape e os erros da Secao 4.1 no meio da string sem
  * uma unica expressao regular gigante. */
\"                  {
                        str_len = 0;
                        str_com_nulo = 0;
                        str_estourou = 0;
                        BEGIN(STRING);
                    }
<STRING>\"          {
                        BEGIN(INITIAL);
                        str_buf[str_len] = '\0';
                        if (str_com_nulo) {
                            microc_yylval.error_msg = "String contem caractere nulo";
                            return UNDEF;
                        }
                        if (str_estourou) {
                            microc_yylval.error_msg = "String muito longa";
                            return UNDEF;
                        }
                        microc_yylval.symbol = tabela_add(str_buf);
                        return STRINGCONST;
                    }
<STRING>\\\r?\n     { linha_atual++; /* quebra de linha escapada: continua a string */ }
<STRING>\\.         { str_add(converte_escape(yytext[1])); }
 /* TP2 ALTERADO: antes era "linha_atual++;". Agora a quebra de linha
  * volta para a entrada (yyless(0)) e e contada pela regra \n de
  * INITIAL, depois. Assim o erro e reportado na linha da string, nao
  * na seguinte (o teste rejeitado_lexico.mc espera a linha 2). */
<STRING>\n          {
                        yyless(0);
                        BEGIN(INITIAL);
                        microc_yylval.error_msg = "String nao terminada";
                        return UNDEF;
                    }
<STRING>\0          { str_com_nulo = 1; }
<STRING>\r          { /* ignora CR de arquivos com fim de linha Windows */ }
<STRING><<EOF>>     {
                        BEGIN(INITIAL);
                        microc_yylval.error_msg = "EOF em string";
                        return UNDEF;
                    }
<STRING>.           { str_add(yytext[0]); }

 /* --- Operadores relacionais e logicos ---------------------------------
  * Para cada par que compartilha prefixo ("==" e "="), as duas formas sao
  * regras separadas: o flex escolhe sempre o casamento mais longo. */
"=="                { return EQ; }
"="                 { return ASSIGN; }
"!="                { return NEQ; }
"!"                 { return NOT; }
"<="                { return LEQ; }
"<"                 { return LT; }
">="                { return GEQ; }
">"                 { return GT; }
"&&"                { return AND; }
"||"                { return OR; }

 /* --- Operadores aritmeticos e simbolos de pontuacao ------------------- */
"+"                 { return PLUS; }
"-"                 { return MINUS; }
"*"                 { return MUL; }
"/"                 { return DIV; }
"%"                 { return MOD; }
";"                 { return SEMICOLON; }
","                 { return COMMA; }
"("                 { return LPAREN; }
")"                 { return RPAREN; }
"{"                 { return LBRACE; }
"}"                 { return RBRACE; }
"["                 { return LBRACKET; }
"]"                 { return RBRACKET; }

 /* --- Caractere invalido -------------------------------------------------
  * Casa com qualquer caractere que nao tenha correspondido a nenhuma
  * regra anterior. Deve ser SEMPRE a ultima regra do arquivo. */
.                   {
                        microc_yylval.error_msg = tabela_add(yytext);
                        return UNDEF;
                    }

%%

/* -----------------------------------------------------------------------
 * 4. SUB-ROTINAS DO USUARIO
 * ------------------------------------------------------------------- */

/* yylex: envolve a funcao gerada pelo flex (lex_interno) para lembrar o
 * ultimo token devolvido, informacao usada na regra de inteiro negativo. */
int yylex(void) {
    microc_yylval.symbol = NULL;
    int t = lex_interno();
    ultimo_token = t;
    return t;
}

/* >>>>> TP2 ALTERADO: yywrap() removida (substituida por %option
 * noyywrap); microc_tabela_liberar() acrescentada; main() de teste e
 * nome_token[] envolvidos por #ifdef MICROC_LEXER_MAIN, para nao
 * colidir com o main() do analisador gerado. */

/* Libera a tabela de strings e reinicia o estado do scanner. Chamada por
 * scanner_finalizar() (scanner.c). */
void microc_tabela_liberar(void) {
    for (int i = 0; i < tabela_tam; i++)
        free(tabela_strings[i]);
    free(tabela_strings);
    tabela_strings = NULL;
    tabela_tam = tabela_cap = 0;
    ultimo_token = -1;
}

#ifdef MICROC_LEXER_MAIN
/* Nomes dos tokens, usados apenas pelo main() de teste abaixo para
 * imprimir o tipo de cada token de forma legivel. Mantenha esta lista
 * na MESMA ORDEM do enum TokenType (tokens.h). */
static const char *nome_token[] = {
    "UNDEF", "ID", "END_OF_FILE",
    "INTEGERCONST", "CHARCONST", "STRINGCONST",
    "PLUS", "MINUS", "MUL", "DIV", "MOD",
    "EQ", "NEQ", "LT", "GT", "LEQ", "GEQ", "AND", "OR", "NOT",
    "ASSIGN", "SEMICOLON", "COMMA", "LPAREN", "RPAREN",
    "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
    "MAIN", "IF", "ELSE", "FOR", "RETURN", "INT", "CHAR", "PRINT"
};

/* main() de teste do TP1: le o arquivo passado como argumento e imprime,
 * para cada token reconhecido, seu tipo, lexema e linha. No TP2 quem
 * chama o scanner e o analisador gerado (via scanner.c), por isso este
 * main() so e compilado com -DMICROC_LEXER_MAIN. */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo.mc>\n", argv[0]);
        return 1;
    }

    FILE *arquivo_fonte = fopen(argv[1], "r");
    if (!arquivo_fonte) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'\n", argv[1]);
        return 1;
    }
    yyin = arquivo_fonte;
    /* stdout linha a linha, para que erros (stderr) e tokens (stdout)
     * saiam na ordem em que ocorreram quando redirecionados juntos. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    int tipo;
    while ((tipo = yylex()) != END_OF_FILE) {
        if (tipo == UNDEF) {
            fprintf(stderr, "ERRO LEXICO (linha %d): %s\n",
                    linha_atual, microc_yylval.error_msg);
            continue;
        }
        /* Para tokens com atributo (ID e constantes) imprime o valor ja
         * convertido e guardado na tabela de strings; para os demais, o
         * proprio yytext. */
        printf("Token: tipo = %-13s lexema = '%s'  linha = %d\n",
               nome_token[tipo],
               microc_yylval.symbol ? microc_yylval.symbol : yytext,
               linha_atual);
    }

    fclose(arquivo_fonte);
    microc_tabela_liberar();   /* TP2 ALTERADO */
    return 0;
}
#endif /* MICROC_LEXER_MAIN */
/* <<<<< TP2 FIM */
