/*
 * verificacao.c - Verificacoes semanticas da gramatica (etapa 2).
 *
 * Ja implementados (use como modelo):
 *   - verif_indefinidos: nao terminais usados mas nao definidos;
 *   - verif_terminais:   terminais nomeados sem declaracao.
 *
 * [TP2] Implementados (trecho marcado "TP2 ALTERADO"):
 *   - verif_inalcancaveis      (aviso)
 *   - verif_improdutivos       (erro)
 *   - verif_recursao_esquerda  (erro; direta e indireta)
 *
 * A duplicidade de definicoes ja e detectada durante a leitura
 * (ebnf_parser.c).
 */
#include <stdlib.h>
#include <string.h>
#include "verificacao.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */

static int verif_indefinidos(Gramatica *g)
{
    int i, erros = 0;
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL) {
            diag_erro(g->nts[i].linha, g->nts[i].coluna,
                      "nao terminal %s usado mas nao definido", g->nts[i].nome);
            erros++;
        }
    }
    return erros;
}

static int verif_terminais(Gramatica *g, int modo_microc)
{
    int i, erros = 0, carregado = 0;
    Terminal *t;

    if (modo_microc)
        carregado = microc_carregar_tokens_h(g->tokens_externos) == 0;

    for (i = 1; i < g->nterms; i++) {
        t = &g->terms[i];
        if (modo_microc) {
            if (t->literal && microc_nome_literal(t->nome) == NULL) {
                diag_erro(t->linha, t->coluna, "literal %s nao corresponde a nenhum "
                          "token de Micro C", t->exibicao);
                erros++;
            } else if (!t->literal && carregado && !microc_token_existe(t->nome)) {
                diag_erro(t->linha, t->coluna, "terminal %s nao existe na enumeracao "
                          "TokenType de %s", t->nome, g->tokens_externos);
                erros++;
            }
        } else if (!t->declarado) {
            diag_erro(t->linha, t->coluna, "terminal %s nao declarado em TOKENS", t->nome);
            erros++;
        }
    }
    return erros;
}

/* ------------------------------------------------------------------ */
/* A implementar                                                      */
/* ------------------------------------------------------------------ */

/* >>>>> TP2 ALTERADO: verif_inalcancaveis, verif_improdutivos e
 * verif_recursao_esquerda (eram stubs que retornavam 0), com as
 * auxiliares marcar_usados, expr_produtiva e canto_esquerdo. */

/* ---- inalcancaveis ---- */

/* Contexto de marcar_usados: vetor de marcas e flag "algo mudou". */
typedef struct {
    int *marcado;
    int mudou;
} CtxAlcance;

/* Visitante de expr_percorrer: marca cada nao terminal usado. */
static void marcar_usados(Expr *e, void *ctx)
{
    CtxAlcance *c = (CtxAlcance *) ctx;
    if (e->tipo == E_NAOTERM && !c->marcado[e->simbolo]) {
        c->marcado[e->simbolo] = 1;
        c->mudou = 1;
    }
}

/* AVISO para cada nao terminal que nao pode ser alcancado a partir do
 * simbolo inicial. Ponto fixo: marca o inicial e, enquanto algo mudar,
 * marca todo nao terminal usado na regra de um nao terminal marcado.
 * Avisos nao contam como erros: retorna sempre 0. */
static int verif_inalcancaveis(Gramatica *g)
{
    int i;
    CtxAlcance c;

    c.marcado = (int *) xcalloc((size_t) g->nnts, sizeof(int));
    c.marcado[g->inicial] = 1;
    do {
        c.mudou = 0;
        for (i = 0; i < g->nnts; i++)
            if (c.marcado[i] && g->nts[i].regra != NULL)
                expr_percorrer(g->nts[i].regra, marcar_usados, &c);
    } while (c.mudou);

    for (i = 0; i < g->nnts; i++)
        if (!c.marcado[i] && g->nts[i].regra != NULL)
            diag_aviso(g->nts[i].linha_def, 0,
                       "nao terminal %s inalcancavel a partir do simbolo inicial %s",
                       g->nts[i].nome, g->nts[g->inicial].nome);
    free(c.marcado);
    return 0;
}

/* ---- improdutivos ---- */

/* 1 se e deriva alguma cadeia de terminais, dado o vetor prod[] atual. */
static int expr_produtiva(const Expr *e, const int *prod)
{
    int i;
    switch (e->tipo) {
    case E_TERM:
    case E_VAZIO:
    case E_OPC:      /* [ a ] e { a } sempre podem derivar a cadeia vazia */
    case E_REP:
        return 1;
    case E_NAOTERM:
        return prod[e->simbolo];
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++)
            if (!expr_produtiva(e->filhos[i], prod))
                return 0;
        return 1;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            if (expr_produtiva(e->filhos[i], prod))
                return 1;
        return 0;
    }
    return 0;
}

/* ERRO para cada nao terminal definido que nao deriva nenhuma cadeia de
 * terminais. Ponto fixo: comeca com todos improdutivos e marca como
 * produtivo cada nao terminal cuja regra passe a ser produtiva. */
static int verif_improdutivos(Gramatica *g)
{
    int i, mudou, erros = 0;
    int *prod = (int *) xcalloc((size_t) g->nnts, sizeof(int));

    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++)
            if (!prod[i] && g->nts[i].regra != NULL
                && expr_produtiva(g->nts[i].regra, prod)) {
                prod[i] = 1;
                mudou = 1;
            }
    } while (mudou);

    for (i = 0; i < g->nnts; i++)
        if (!prod[i] && g->nts[i].regra != NULL) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s e improdutivo (nao deriva nenhuma cadeia de "
                      "terminais)", g->nts[i].nome);
            erros++;
        }
    free(prod);
    return erros;
}

/* ---- recursao a esquerda ---- */

/* Marca em linha[] os nao terminais que podem aparecer na extremidade
 * esquerda de e: numa sequencia, os filhos ate o primeiro nao anulavel
 * (assim "a = [ x ] a" conta); em |, [ ] e { }, todos os filhos. */
static void canto_esquerdo(const Expr *e, int *linha)
{
    int i;
    switch (e->tipo) {
    case E_TERM:
    case E_VAZIO:
        break;
    case E_NAOTERM:
        linha[e->simbolo] = 1;
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++) {
            canto_esquerdo(e->filhos[i], linha);
            if (!e->filhos[i]->anulavel)
                break;
        }
        break;
    case E_ALT:
    case E_OPC:
    case E_REP:
        for (i = 0; i < e->nfilhos; i++)
            canto_esquerdo(e->filhos[i], linha);
        break;
    }
}

/* Recursao a esquerda direta (a = a ...) e indireta (a = b ...; b = a ...),
 * inclusive atraves de prefixos anulaveis. Monta a relacao "B pode
 * comecar a regra de A" (matriz direta), faz o fecho transitivo
 * (Warshall) e verifica a diagonal: direta se A comeca a propria regra,
 * indireta se A so volta a si passando por outros nao terminais. */
int verif_recursao_esquerda(Gramatica *g)
{
    int n = g->nnts, i, j, k, erros = 0;
    int *direta = (int *) xcalloc((size_t) n * (size_t) n, sizeof(int));
    int *fecho = (int *) xcalloc((size_t) n * (size_t) n, sizeof(int));

    for (i = 0; i < n; i++)
        if (g->nts[i].regra != NULL)
            canto_esquerdo(g->nts[i].regra, &direta[i * n]);
    memcpy(fecho, direta, (size_t) n * (size_t) n * sizeof(int));
    for (k = 0; k < n; k++)
        for (i = 0; i < n; i++)
            if (fecho[i * n + k])
                for (j = 0; j < n; j++)
                    if (fecho[k * n + j])
                        fecho[i * n + j] = 1;

    for (i = 0; i < n; i++) {
        if (direta[i * n + i]) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s tem recursao a esquerda direta", g->nts[i].nome);
            erros++;
        } else if (fecho[i * n + i]) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s tem recursao a esquerda indireta", g->nts[i].nome);
            erros++;
        }
    }
    free(direta);
    free(fecho);
    return erros;
}
/* <<<<< TP2 FIM */

int verif_basica(Gramatica *g, int modo_microc)
{
    int erros = 0;
    if (g->inicial < 0) {
        diag_erro(0, 0, "a gramatica nao tem nenhuma regra");
        return 1;
    }
    erros += verif_indefinidos(g);
    erros += verif_terminais(g, modo_microc);
    erros += verif_inalcancaveis(g);
    if (erros == 0)
        erros += verif_improdutivos(g);
    return erros;
}
