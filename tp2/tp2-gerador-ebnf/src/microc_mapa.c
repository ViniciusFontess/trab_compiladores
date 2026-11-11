/*
 * microc_mapa.c - Correspondencia com os tokens de Micro C (ver microc_mapa.h).
 *
 * A tabela de literais segue o tokens.h do livro-texto (secao 2.3.1):
 *
 *   UNDEF, ID, END_OF_FILE, INTEGERCONST, CHARCONST, STRINGCONST,
 *   PLUS, MINUS, MUL, DIV, MOD, EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,
 *   ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN, LBRACE, RBRACE,
 *   LBRACKET, RBRACKET, MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
 *
 * Se o seu tokens.h do TP1 usar outros nomes, ajuste a tabela.
 *
 * [TP2] Implementado (trecho marcado "TP2 ALTERADO"):
 * microc_carregar_tokens_h le os nomes da enumeracao TokenType do
 * tokens.h indicado em TOKENS EXTERN (microc/tokens.h).
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microc_mapa.h"
#include "util.h"

static const char *const literais[][2] = {
    { "+",  "PLUS" },      { "-",  "MINUS" },     { "*",  "MUL" },
    { "/",  "DIV" },       { "%",  "MOD" },
    { "==", "EQ" },        { "!=", "NEQ" },       { "<",  "LT" },
    { ">",  "GT" },        { "<=", "LEQ" },       { ">=", "GEQ" },
    { "&&", "AND" },       { "||", "OR" },        { "!",  "NOT" },
    { "=",  "ASSIGN" },    { ";",  "SEMICOLON" }, { ",",  "COMMA" },
    { "(",  "LPAREN" },    { ")",  "RPAREN" },    { "{",  "LBRACE" },
    { "}",  "RBRACE" },    { "[",  "LBRACKET" },  { "]",  "RBRACKET" },
    { "main", "MAIN" },    { "if", "IF" },        { "else", "ELSE" },
    { "for", "FOR" },      { "return", "RETURN" },{ "int", "INT" },
    { "char", "CHAR" },    { "print", "PRINT" }
};

static char **nomes = NULL;
static int nnomes = 0, capnomes = 0;

const char *microc_nome_literal(const char *literal)
{
    size_t i;
    for (i = 0; i < sizeof(literais) / sizeof(literais[0]); i++)
        if (strcmp(literais[i][0], literal) == 0)
            return literais[i][1];
    return NULL;
}

static void adicionar_nome(const char *s, size_t n)
{
    if (nnomes == capnomes) {
        capnomes = capnomes ? 2 * capnomes : 64;
        nomes = (char **) xrealloc(nomes, (size_t) capnomes * sizeof(char *));
    }
    nomes[nnomes++] = xstrndup(s, n);
}

/* >>>>> TP2 ALTERADO: microc_carregar_tokens_h (era stub que so emitia
 * um aviso), com as auxiliares sem_comentarios, eh_inicio_id, eh_id,
 * palavra_igual e extrair_enum. */

/* Copia de src sem comentarios C (barra-asterisco e barra-barra), trocados
 * por espaco. Literais de string/char nao aparecem numa enumeracao e nao
 * precisam de tratamento. */
static char *sem_comentarios(const char *src)
{
    size_t n = strlen(src), i = 0, j = 0;
    char *dst = (char *) xmalloc(n + 1);

    while (i < n) {
        if (src[i] == '/' && src[i + 1] == '*') {
            i += 2;
            while (i < n && !(src[i] == '*' && src[i + 1] == '/'))
                i++;
            i = i < n ? i + 2 : n;
            dst[j++] = ' ';
        } else if (src[i] == '/' && src[i + 1] == '/') {
            while (i < n && src[i] != '\n')
                i++;
            dst[j++] = ' ';
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
    return dst;
}

static int eh_inicio_id(char c) { return isalpha((unsigned char) c) || c == '_'; }
static int eh_id(char c)        { return isalnum((unsigned char) c) || c == '_'; }

/* 1 se a palavra em s (terminada por um nao-identificador) e exatamente w. */
static int palavra_igual(const char *s, const char *w)
{
    size_t n = strlen(w);
    return strncmp(s, w, n) == 0 && !eh_id(s[n]);
}

/* Procura "enum { ... } TokenType" e chama adicionar_nome para cada item
 * da lista, pulando valores explicitos ("X = 3"). Retorna 0 se achou. */
static int extrair_enum(const char *s)
{
    const char *p = s, *abre, *fecha, *q;

    for (; *p != '\0'; p++) {
        if (!palavra_igual(p, "enum") || (p > s && eh_id(p[-1])))
            continue;
        abre = strchr(p, '{');
        if (abre == NULL)
            return -1;
        fecha = strchr(abre, '}');
        if (fecha == NULL)
            return -1;
        /* nome depois do '}': precisa ser TokenType */
        q = fecha + 1;
        while (isspace((unsigned char) *q))
            q++;
        if (!palavra_igual(q, "TokenType")) {
            p = fecha;
            continue;
        }
        /* itens da enumeracao: NOME [ = valor ] { , NOME [ = valor ] } */
        q = abre + 1;
        while (q < fecha) {
            const char *ini;
            while (q < fecha && !eh_inicio_id(*q) && *q != '=')
                q++;
            if (q >= fecha)
                break;
            if (*q == '=') {           /* pula o valor ate a proxima virgula */
                while (q < fecha && *q != ',')
                    q++;
                continue;
            }
            ini = q;
            while (q < fecha && eh_id(*q))
                q++;
            adicionar_nome(ini, (size_t) (q - ini));
        }
        return nnomes > 0 ? 0 : -1;
    }
    return -1;
}

int microc_carregar_tokens_h(const char *caminho)
{
    char *fonte, *limpo;
    int r;

    microc_liberar();
    fonte = ler_arquivo(caminho);
    if (fonte == NULL) {
        diag_aviso(0, 0, "nao foi possivel abrir %s: nomes dos tokens nao verificados",
                   caminho);
        return -1;
    }
    limpo = sem_comentarios(fonte);
    r = extrair_enum(limpo);
    if (r != 0)
        diag_aviso(0, 0, "enumeracao TokenType nao encontrada em %s: nomes dos tokens "
                   "nao verificados", caminho);
    free(limpo);
    free(fonte);
    return r;
}
/* <<<<< TP2 FIM */

int microc_token_existe(const char *nome)
{
    int i;
    for (i = 0; i < nnomes; i++)
        if (strcmp(nomes[i], nome) == 0)
            return 1;
    return 0;
}

void microc_liberar(void)
{
    int i;
    for (i = 0; i < nnomes; i++)
        free(nomes[i]);
    free(nomes);
    nomes = NULL;
    nnomes = capnomes = 0;
}
