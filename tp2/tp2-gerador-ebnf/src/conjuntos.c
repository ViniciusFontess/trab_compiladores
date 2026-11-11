/*
 * conjuntos.c - Calculo de anulavel, FIRST e FOLLOW (etapa 3).
 *
 * Fornecido: conj_iniciar e conj_imprimir.
 *
 * [TP2] Implementados (trecho marcado "TP2 ALTERADO"):
 * conj_anulavel, conj_first e conj_follow sao calculados por iteracao ate
 * ponto fixo: repete-se uma passada sobre todas as regras enquanto algum
 * conjunto (ou flag) mudar. bitset_unir devolve 1 quando o destino muda.
 */
#include <stdio.h>
#include <string.h>
#include "conjuntos.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */

static void iniciar_expr(Expr *e, void *ctx)
{
    int n = *(int *) ctx;
    bitset_liberar(&e->first);
    bitset_liberar(&e->follow);
    bitset_iniciar(&e->first, n);
    bitset_iniciar(&e->follow, n);
    e->anulavel = 0;
}

void conj_iniciar(Gramatica *g)
{
    int i, n = g->nterms;
    for (i = 0; i < g->nnts; i++) {
        bitset_liberar(&g->nts[i].first);
        bitset_liberar(&g->nts[i].follow);
        bitset_iniciar(&g->nts[i].first, n);
        bitset_iniciar(&g->nts[i].follow, n);
        g->nts[i].anulavel = 0;
        expr_percorrer(g->nts[i].regra, iniciar_expr, &n);
    }
}

void conj_imprimir(FILE *f, const Gramatica *g)
{
    int i;
    size_t largura = strlen("NAO TERMINAL");
    for (i = 0; i < g->nnts; i++)
        if (strlen(g->nts[i].nome) > largura)
            largura = strlen(g->nts[i].nome);
    fprintf(f, "%-*s  ANULAVEL  FIRST / FOLLOW\n", (int) largura, "NAO TERMINAL");
    for (i = 0; i < g->nnts; i++) {
        fprintf(f, "%-*s  %-8s  FIRST  = ", (int) largura, g->nts[i].nome,
                g->nts[i].anulavel ? "sim" : "nao");
        gram_imprimir_conjunto(f, g, &g->nts[i].first);
        fprintf(f, "\n%-*s            FOLLOW = ", (int) largura, "");
        gram_imprimir_conjunto(f, g, &g->nts[i].follow);
        fputc('\n', f);
    }
}

/* ------------------------------------------------------------------ */
/* A implementar                                                      */
/* ------------------------------------------------------------------ */

/* >>>>> TP2 ALTERADO: conj_anulavel, conj_first e conj_follow (eram
 * stubs vazios), com as auxiliares anulavel_expr, first_expr e
 * follow_expr. */

/* Todas as funcoes seguem o mesmo padrao: uma funcao recursiva visita a
 * arvore Expr de uma regra, atualiza os atributos de cada no e devolve 1
 * se algo mudou. A funcao publica repete passadas sobre todas as regras
 * ate uma passada inteira nao mudar nada (ponto fixo). Nao terminais
 * sem regra (indefinidos) sao ignorados: ja foram reportados na etapa 2. */

/* ---- anulavel ---- */

/* Calcula e->anulavel para e e seus descendentes. Retorna 1 se mudou. */
static int anulavel_expr(Gramatica *g, Expr *e)
{
    int i, mudou = 0, novo = 0;

    /* filhos primeiro: o valor do no depende deles */
    for (i = 0; i < e->nfilhos; i++)
        mudou |= anulavel_expr(g, e->filhos[i]);

    switch (e->tipo) {
    case E_TERM:
        novo = 0;
        break;
    case E_VAZIO:
    case E_OPC:
    case E_REP:
        novo = 1;
        break;
    case E_NAOTERM:
        novo = g->nts[e->simbolo].anulavel;
        break;
    case E_SEQ:
        novo = 1;
        for (i = 0; i < e->nfilhos; i++)
            if (!e->filhos[i]->anulavel)
                novo = 0;
        break;
    case E_ALT:
        novo = 0;
        for (i = 0; i < e->nfilhos; i++)
            if (e->filhos[i]->anulavel)
                novo = 1;
        break;
    }
    if (novo != e->anulavel) {
        e->anulavel = novo;
        mudou = 1;
    }
    return mudou;
}

void conj_anulavel(Gramatica *g)
{
    int i, mudou;
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            Expr *r = g->nts[i].regra;
            if (r == NULL)
                continue;
            mudou |= anulavel_expr(g, r);
            if (r->anulavel != g->nts[i].anulavel) {
                g->nts[i].anulavel = r->anulavel;
                mudou = 1;
            }
        }
    } while (mudou);
}

/* ---- FIRST ---- */

/* Calcula e->first para e e TODOS os seus descendentes (inclusive os
 * filhos de uma sequencia que nao contribuem para o FIRST dela, pois o
 * gerador de codigo usa o FIRST de cada filho). Retorna 1 se mudou. */
static int first_expr(Gramatica *g, Expr *e)
{
    int i, mudou = 0;

    for (i = 0; i < e->nfilhos; i++)
        mudou |= first_expr(g, e->filhos[i]);

    switch (e->tipo) {
    case E_TERM:
        if (!bitset_tem(&e->first, e->simbolo)) {
            bitset_add(&e->first, e->simbolo);
            mudou = 1;
        }
        break;
    case E_VAZIO:
        break;
    case E_NAOTERM:
        mudou |= bitset_unir(&e->first, &g->nts[e->simbolo].first);
        break;
    case E_SEQ:
        /* FIRST(a1) U FIRST(a2) U ... ate o primeiro ai nao anulavel */
        for (i = 0; i < e->nfilhos; i++) {
            mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
            if (!e->filhos[i]->anulavel)
                break;
        }
        break;
    case E_ALT:
    case E_OPC:
    case E_REP:
        for (i = 0; i < e->nfilhos; i++)
            mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
        break;
    }
    return mudou;
}

void conj_first(Gramatica *g)
{
    int i, mudou;
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            Expr *r = g->nts[i].regra;
            if (r == NULL)
                continue;
            mudou |= first_expr(g, r);
            mudou |= bitset_unir(&g->nts[i].first, &r->first);
        }
    } while (mudou);
}

/* ---- FOLLOW ---- */

/* Propaga e->follow (ja atualizado pelo pai) para os filhos de e e, nos
 * E_NAOTERM, para o FOLLOW do nao terminal. Retorna 1 se mudou. */
static int follow_expr(Gramatica *g, Expr *e, Bitset *resto)
{
    int i, mudou = 0;

    switch (e->tipo) {
    case E_TERM:
    case E_VAZIO:
        break;
    case E_NAOTERM:
        mudou |= bitset_unir(&g->nts[e->simbolo].follow, &e->follow);
        break;
    case E_SEQ:
        /* Da direita para a esquerda, resto = FIRST(ai+1 ... an), mais
         * follow(seq) enquanto o sufixo ai+1 ... an for anulavel. */
        bitset_copiar(resto, &e->follow);
        for (i = e->nfilhos - 1; i >= 0; i--) {
            Expr *a = e->filhos[i];
            mudou |= bitset_unir(&a->follow, resto);
            if (a->anulavel)
                bitset_unir(resto, &a->first);
            else
                bitset_copiar(resto, &a->first);
        }
        break;
    case E_ALT:
    case E_OPC:
        for (i = 0; i < e->nfilhos; i++)
            mudou |= bitset_unir(&e->filhos[i]->follow, &e->follow);
        break;
    case E_REP:
        /* depois de { a } pode vir outra repeticao de a */
        mudou |= bitset_unir(&e->filhos[0]->follow, &e->follow);
        mudou |= bitset_unir(&e->filhos[0]->follow, &e->filhos[0]->first);
        break;
    }

    /* resto e reaproveitado pelos filhos: a passada da sequencia acima
     * ja terminou de usa-lo */
    for (i = 0; i < e->nfilhos; i++)
        mudou |= follow_expr(g, e->filhos[i], resto);
    return mudou;
}

void conj_follow(Gramatica *g)
{
    int i, mudou;
    Bitset resto;

    bitset_iniciar(&resto, g->nterms);
    if (g->inicial >= 0)
        bitset_add(&g->nts[g->inicial].follow, TERM_FIM);
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            Expr *r = g->nts[i].regra;
            if (r == NULL)
                continue;
            mudou |= bitset_unir(&r->follow, &g->nts[i].follow);
            mudou |= follow_expr(g, r, &resto);
        }
    } while (mudou);
    bitset_liberar(&resto);
}
/* <<<<< TP2 FIM */
