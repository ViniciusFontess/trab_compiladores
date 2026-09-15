%{
/*
 * microc.flex
 *
 * Compilacao:
 *      flex microc.flex
 *      gcc lex.yy.c -o lexer -lfl
 *
 * Uso:
 *      ./lexer arquivo.mc
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    UNDEF, ID, END_OF_FILE,
    INTEGERCONST, CHARCONST, STRINGCONST,
    PLUS, MINUS, MUL, DIV, MOD,
    EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,
    ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN,
    LBRACE, RBRACE, LBRACKET, RBRACKET,
    MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
} TokenType;

static const char *nome_token[] = {
    "UNDEF", "ID", "END_OF_FILE",
    "INTEGERCONST", "CHARCONST", "STRINGCONST",
    "PLUS", "MINUS", "MUL", "DIV", "MOD",
    "EQ", "NEQ", "LT", "GT", "LEQ", "GEQ", "AND", "OR", "NOT",
    "ASSIGN", "SEMICOLON", "COMMA", "LPAREN", "RPAREN",
    "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
    "MAIN", "IF", "ELSE", "FOR", "RETURN", "INT", "CHAR", "PRINT"
};

typedef struct {
    char *texto;
    char *erro;
} ValorToken;

static ValorToken valor_atual;

int linha_atual = 1;

#define TABELA_MAX 1024
static char *tabela[TABELA_MAX];
static int   tabela_n = 0;

static char *tabela_guarda(const char *s) {
    int i;
    for (i = 0; i < tabela_n; i++) {
        if (strcmp(tabela[i], s) == 0) {
            return tabela[i];
        }
    }
    if (tabela_n < TABELA_MAX) {
        tabela[tabela_n] = strdup(s);
        tabela_n++;
        return tabela[tabela_n - 1];
    }
    return strdup(s);
}

static int ultimo_token = -1;

static int pode_terminar_expressao(int t) {
    return t == ID || t == INTEGERCONST || t == CHARCONST || t == STRINGCONST
        || t == RPAREN || t == RBRACKET;
}

static char converte_escape(char c) {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case '0': return '\0';
        default:  return c;
    }
}

#define STR_MAX 4096
static char str_buf[STR_MAX];
static int  str_len;
static int  str_tem_nulo;

static void str_add(char c) {
    if (str_len < STR_MAX - 1) {
        str_buf[str_len++] = c;
    }
}
%}

DIGITO   [0-9]
LETRA    [a-zA-Z_]
ALFANUM  [a-zA-Z0-9_]

%x COMENTARIO
%x STRING

%%

<INITIAL><<EOF>>     { return END_OF_FILE; }

\n                   { linha_atual++; }
[ \t\r]+             { /* ignorados */ }

"//".*               { /* comentario de linha, descartado */ }

"/*"                 { BEGIN(COMENTARIO); }
<COMENTARIO>"*/"     { BEGIN(INITIAL); }
<COMENTARIO>\n       { linha_atual++; }
<COMENTARIO>.        { }
<COMENTARIO><<EOF>>  {
                         BEGIN(INITIAL);
                         valor_atual.erro = "EOF em comentario";
                         return UNDEF;
                      }

"*/"                 {
                         valor_atual.erro = "Comentario nao iniciado";
                         return UNDEF;
                      }

 /* TODO(aluno): reconhecer palavras reservadas -- identificador casa
  * primeiro (maximal munch), so depois comparamos com strcmp() na
  * tabela de reservadas. valor_atual.texto (o campo da ValorToken
  * pro lexema) usa tabela_guarda(), que copia com strdup() e
  * reaproveita o ponteiro se o mesmo texto ja apareceu antes. */
{LETRA}{ALFANUM}*    {
                         static const struct { const char *palavra; TokenType tipo; } reservadas[] = {
                             { "main",   MAIN   }, { "if",     IF     },
                             { "else",  ELSE   }, { "for",    FOR    },
                             { "return", RETURN }, { "int",    INT    },
                             { "char",   CHAR   }, { "print",  PRINT  },
                         };
                         const int n = sizeof(reservadas) / sizeof(reservadas[0]);
                         int i;
                         for (i = 0; i < n; i++) {
                             if (strcmp(yytext, reservadas[i].palavra) == 0) {
                                 return reservadas[i].tipo;
                             }
                         }
                         valor_atual.texto = tabela_guarda(yytext);
                         return ID;
                      }

{DIGITO}+            {
                         valor_atual.texto = tabela_guarda(yytext);
                         return INTEGERCONST;
                      }

 /* TODO(aluno): inteiro negativo vs. subtracao -- decide olhando o
  * token ANTERIOR (ultimo_token/pode_terminar_expressao). Se era
  * subtracao, yyless(1) "devolve" os digitos pro flex reler depois,
  * ficando so o '-' casado nesta chamada. */
"-"{DIGITO}+         {
                         if (pode_terminar_expressao(ultimo_token)) {
                             yyless(1);
                             return MINUS;
                         }
                         valor_atual.texto = tabela_guarda(yytext);
                         return INTEGERCONST;
                      }

 /* TODO(aluno): reconhecer CHARCONST e seus erros. converte_escape()
  * traduz o caractere depois da barra (\t, \n...); o "default" dela
  * resolve \" e \\ devolvendo o proprio caractere, sem um case pra
  * cada. */
"'"([^'\\\n]|\\.)"'" {
                         char v[2];
                         v[0] = (yytext[1] == '\\') ? converte_escape(yytext[2]) : yytext[1];
                         v[1] = '\0';
                         valor_atual.texto = tabela_guarda(v);
                         return CHARCONST;
                      }
"'"[^'\n]*"'"        {
                         valor_atual.erro = "Constante de caractere invalida";
                         return UNDEF;
                      }
"'"[^'\n]*           {
                         valor_atual.erro = "Constante de caractere nao terminada";
                         return UNDEF;
                      }

 /* TODO(aluno): reconhecer STRINGCONST com escapes e os erros da
  * Secao 4.1. str_buf/str_add montam o valor JA convertido -- nao e
  * o mesmo texto que yytext mostraria, que teria os escapes "crus"
  * (barra e letra separados, em vez do caractere de verdade). */
\"                   {
                         str_len = 0;
                         str_tem_nulo = 0;
                         BEGIN(STRING);
                      }
<STRING>\"           {
                         BEGIN(INITIAL);
                         str_buf[str_len] = '\0';
                         if (str_tem_nulo) {
                             valor_atual.erro = "String contem caractere nulo";
                             return UNDEF;
                         }
                         valor_atual.texto = tabela_guarda(str_buf);
                         return STRINGCONST;
                      }
<STRING>\\.          { str_add(converte_escape(yytext[1])); }
<STRING>\0           { str_tem_nulo = 1; }
<STRING>\n           {
                         linha_atual++;
                         BEGIN(INITIAL);
                         valor_atual.erro = "String nao terminada";
                         return UNDEF;
                      }
<STRING><<EOF>>      {
                         BEGIN(INITIAL);
                         valor_atual.erro = "EOF em string";
                         return UNDEF;
                      }
<STRING>.            { str_add(yytext[0]); }

"=="                 { return EQ; }
"="                  { return ASSIGN; }

 /* TODO(aluno): completar os demais operadores de prefixo
  * compartilhado, seguindo o exemplo de "==" e "=" acima -- o flex
  * sempre casa o mais longo, entao duas regras separadas bastam
  * (nao precisa de logica extra igual no caso do numero negativo). */
"!="                 { return NEQ; }
"!"                  { return NOT; }
"<="                 { return LEQ; }
"<"                  { return LT; }
">="                 { return GEQ; }
">"                  { return GT; }
"&&"                 { return AND; }
"||"                 { return OR; }

"+"                  { return PLUS; }
"-"                  { return MINUS; }
"*"                  { return MUL; }
"/"                  { return DIV; }
"%"                  { return MOD; }
";"                  { return SEMICOLON; }
","                  { return COMMA; }
"("                  { return LPAREN; }
")"                  { return RPAREN; }
"{"                  { return LBRACE; }
"}"                  { return RBRACE; }
"["                  { return LBRACKET; }
"]"                  { return RBRACKET; }

.                    {
                         valor_atual.erro = tabela_guarda(yytext);
                         return UNDEF;
                      }

%%

int yywrap(void) {
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo.mc>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Nao foi possivel abrir o arquivo: %s\n", argv[1]);
        return 1;
    }
    yyin = f;

    int tipo;
    for (;;) {
        valor_atual.texto = NULL;
        valor_atual.erro  = NULL;

        tipo = yylex();
        if (tipo == END_OF_FILE) break;

        if (tipo == UNDEF) {
            fprintf(stderr, "ERRO LEXICO (linha %d): %s\n", linha_atual, valor_atual.erro);
        } else {
            printf("Token: tipo = %-13s lexema = '%s'  linha = %d\n",
                   nome_token[tipo],
                   valor_atual.texto ? valor_atual.texto : yytext,
                   linha_atual);
        }

        ultimo_token = tipo;
    }

    fclose(f);
    return 0;
}
