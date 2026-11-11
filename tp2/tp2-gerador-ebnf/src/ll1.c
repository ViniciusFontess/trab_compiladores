/*
 * ll1.c - Verificacao da condicao LL(1) (etapa 4).
 *
 * Condicoes (secao 3 do enunciado):
 *   1. Em a1 | a2 | ... | an, os FIRST das alternativas sao disjuntos dois
 *      a dois e no maximo uma alternativa e anulavel.
 *   2. Se uma alternativa ai e anulavel, FOLLOW(alternativa) e disjunto
 *      dos FIRST das demais.
 *   3. Em [ a ] e { a }, a nao e anulavel e FIRST(a) e disjunto do
 *      FOLLOW da construcao.
 *
 * Violacoes de 1, 2 e da primeira parte de 3 sao ERROS. A violacao de
 * FIRST(a) inter FOLLOW nao vazio em [ a ] ou { a } e reportada como
 * AVISO e resolvida de forma gulosa: o analisador gerado entra na
 * construcao sempre que o token estiver em FIRST(a). E o caso classico
 * do "else pendente":  if = "if" "(" expr ")" cmd [ "else" cmd ] ;
 * em que o else fica com o if mais proximo, como em C.
 *
 * Fornecido: reportar (formata as mensagens).
 * [TP2] Implementados (trecho marcado "TP2 ALTERADO"): verificar_alt, verificar_opc_rep, verificar_expr
 * e ll1_verificar.
 */
#include <stdarg.h>
#include <stdio.h>
#include "ll1.h"
#include "util.h"

/* Reporta um conflito na regra do nao terminal nt, na posicao de e.
 * comum (pode ser NULL) e o conjunto de tokens em conflito. */
static void reportar(const Gramatica *g, int nt, const Expr *e, int eh_aviso,
                     const Bitset *comum, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d:%d: %s: conflito LL(1) em %s: ", g->arquivo, e->linha,
            e->coluna, eh_aviso ? "aviso" : "erro", g->nts[nt].nome);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (comum != NULL) {
        fprintf(stderr, " ");
        gram_imprimir_conjunto(stderr, g, comum);
    }
    fputc('\n', stderr);
}

/* >>>>> TP2 ALTERADO: ll1_verificar (era stub que retornava 0), com as
 * auxiliares verificar_alt, verificar_opc_rep e verificar_expr. */

/* Condicoes 1 e 2 num no E_ALT. Retorna o numero de erros. */
static int verificar_alt(const Gramatica *g, int nt, const Expr *e, Bitset *comum)
{
    int i, j, erros = 0, anulavel = -1;

    for (i = 0; i < e->nfilhos; i++) {
        const Expr *ai = e->filhos[i];

        /* 1a. FIRST disjuntos dois a dois */
        for (j = i + 1; j < e->nfilhos; j++) {
            bitset_intersecao(comum, &ai->first, &e->filhos[j]->first);
            if (!bitset_vazio(comum)) {
                reportar(g, nt, e, 0, comum, "alternativas %d e %d comecam com",
                         i + 1, j + 1);
                erros++;
            }
        }

        if (!ai->anulavel)
            continue;

        /* 1b. no maximo uma alternativa anulavel */
        if (anulavel >= 0) {
            reportar(g, nt, e, 0, NULL, "alternativas %d e %d sao ambas anulaveis",
                     anulavel + 1, i + 1);
            erros++;
        } else {
            anulavel = i;
        }

        /* 2. alternativa anulavel: FOLLOW dela disjunto dos FIRST das demais */
        for (j = 0; j < e->nfilhos; j++) {
            if (j == i)
                continue;
            bitset_intersecao(comum, &ai->follow, &e->filhos[j]->first);
            if (!bitset_vazio(comum)) {
                reportar(g, nt, e, 0, comum, "alternativa %d e anulavel e seu FOLLOW "
                         "intersecta o FIRST da alternativa %d em", i + 1, j + 1);
                erros++;
            }
        }
    }
    return erros;
}

/* Condicao 3 num no E_OPC ou E_REP. A parte "a anulavel" e erro; a
 * intersecao FIRST(a) / FOLLOW e aviso (resolvida de forma gulosa). */
static int verificar_opc_rep(const Gramatica *g, int nt, const Expr *e, Bitset *comum)
{
    const Expr *a = e->filhos[0];
    const char *construcao = e->tipo == E_OPC ? "[ ]" : "{ }";

    if (a->anulavel) {
        reportar(g, nt, e, 0, NULL, "o conteudo de %s e anulavel", construcao);
        return 1;
    }
    bitset_intersecao(comum, &a->first, &e->follow);
    if (!bitset_vazio(comum))
        reportar(g, nt, e, 1, comum, "FIRST e FOLLOW de %s se intersectam (resolvido "
                 "de forma gulosa) em", construcao);
    return 0;
}

/* Percorre e em pre-ordem verificando cada |, [ ] e { }. */
static int verificar_expr(const Gramatica *g, int nt, const Expr *e, Bitset *comum)
{
    int i, erros = 0;

    if (e->tipo == E_ALT)
        erros += verificar_alt(g, nt, e, comum);
    else if (e->tipo == E_OPC || e->tipo == E_REP)
        erros += verificar_opc_rep(g, nt, e, comum);

    for (i = 0; i < e->nfilhos; i++)
        erros += verificar_expr(g, nt, e->filhos[i], comum);
    return erros;
}

/* Verifica todas as regras e retorna o numero de ERROS (avisos nao
 * contam). Mensagem de exemplo:
 *   expr.ebnf:7:10: erro: conflito LL(1) em stmt: alternativas 1 e 2 comecam com {ID} */
int ll1_verificar(const Gramatica *g)
{
    int i, erros = 0;
    Bitset comum;

    bitset_iniciar(&comum, g->nterms);
    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL)
            erros += verificar_expr(g, i, g->nts[i].regra, &comum);
    bitset_liberar(&comum);
    return erros;
}
/* <<<<< TP2 FIM */
