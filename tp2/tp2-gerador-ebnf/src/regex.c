/*
 * [TP2] ARQUIVO NOVO: nao existia no codigo-base. Todo o conteudo foi
 * escrito no TP2 para implementar gen_casamento_regex (codegen.c).
 *
 * regex.c - Expressao regular -> AFN (Thompson) -> AFD (subconjuntos)
 * -> codigo C. Ver regex.h.
 *
 * 1. Analise descendente recursiva da expressao:
 *        alt    = concat { "|" concat }
 *        concat = { repet }
 *        repet  = atomo { "*" | "+" | "?" }
 *        atomo  = "(" alt ")" | "[" classe "]" | "." | "\" c | c
 *    cada funcao devolve um fragmento de AFN (estado inicial e final).
 * 2. Construcao de subconjuntos: cada estado do AFD e um conjunto de
 *    estados do AFN fechado por transicoes vazias.
 * 3. Emissao: um switch sobre o estado do AFD, com as transicoes
 *    agrupadas em faixas de caracteres, guardando a ultima posicao em
 *    que o automato passou por um estado final (maior prefixo).
 *
 * O caractere '\0' nunca tem transicao: ele marca o fim da entrada.
 */
#include <stdlib.h>
#include <string.h>
#include "regex.h"
#include "bitset.h"
#include "util.h"

#define MAX_ESTADOS_AFD 4000   /* protecao contra explosao de estados */

/* ------------------------------------------------------------------ */
/* AFN                                                                */
/* ------------------------------------------------------------------ */

/* Um estado do AFN tem OU uma aresta por classe de caracteres (prox1)
 * OU ate duas arestas vazias (prox1, prox2). -1 = sem aresta. */
typedef struct {
    int por_classe;
    int prox1, prox2;
    unsigned char classe[32];   /* 256 bits: um por caractere */
} EstadoAfn;

typedef struct {
    EstadoAfn *v;
    int n, cap;
    const char *re;      /* expressao sendo analisada */
    size_t pos;
    const char *erro;    /* primeira mensagem de erro (NULL = sem erro) */
} Afn;

typedef struct {
    int ini, fim;        /* fim nunca tem arestas ao ser devolvido */
} Frag;

static void classe_add(unsigned char *cl, int c)  { cl[c >> 3] |= (unsigned char) (1u << (c & 7)); }
static int  classe_tem(const unsigned char *cl, int c) { return (cl[c >> 3] >> (c & 7)) & 1; }

static int novo_estado(Afn *a)
{
    if (a->n == a->cap) {
        a->cap = a->cap ? 2 * a->cap : 64;
        a->v = (EstadoAfn *) xrealloc(a->v, (size_t) a->cap * sizeof(EstadoAfn));
    }
    memset(&a->v[a->n], 0, sizeof(EstadoAfn));
    a->v[a->n].prox1 = a->v[a->n].prox2 = -1;
    return a->n++;
}

static void ligar_vazio(Afn *a, int de, int para)
{
    if (a->v[de].prox1 < 0)
        a->v[de].prox1 = para;
    else
        a->v[de].prox2 = para;
}

static void erro(Afn *a, const char *msg)
{
    if (a->erro == NULL)
        a->erro = msg;
}

/* Fragmento que casa um caractere da classe cl. */
static Frag frag_classe(Afn *a, const unsigned char *cl)
{
    Frag f;
    f.ini = novo_estado(a);
    f.fim = novo_estado(a);
    a->v[f.ini].por_classe = 1;
    a->v[f.ini].prox1 = f.fim;
    memcpy(a->v[f.ini].classe, cl, 32);
    return f;
}

/* Fragmento que casa so a cadeia vazia. */
static Frag frag_vazio(Afn *a)
{
    Frag f;
    f.ini = f.fim = novo_estado(a);
    return f;
}

/* ------------------------------------------------------------------ */
/* Analise da expressao                                               */
/* ------------------------------------------------------------------ */

static char atual(const Afn *a) { return a->re[a->pos]; }

/* Caractere de escape: \n \t \r viram controle; os demais (\\ \/ \. ...)
 * sao o proprio caractere. */
static int escape(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'f': return '\f';
    case 'v': return '\v';
    default:  return (unsigned char) c;
    }
}

/* Le um caractere (possivelmente escapado) dentro de uma classe. */
static int char_da_classe(Afn *a)
{
    int c;
    if (atual(a) == '\\') {
        a->pos++;
        if (atual(a) == '\0') {
            erro(a, "barra invertida no fim da expressao");
            return 0;
        }
        c = escape(atual(a));
    } else {
        c = (unsigned char) atual(a);
    }
    a->pos++;
    return c;
}

/* "[" ja consumido: le ate "]" e devolve o fragmento. */
static Frag parse_classe(Afn *a)
{
    unsigned char cl[32];
    int negada = 0, primeiro = 1, lo, hi, c, i;

    memset(cl, 0, sizeof cl);
    if (atual(a) == '^') {
        negada = 1;
        a->pos++;
    }
    for (;;) {
        if (atual(a) == '\0') {
            erro(a, "classe de caracteres sem ']'");
            return frag_vazio(a);
        }
        if (atual(a) == ']' && !primeiro)   /* "[]...]": ']' inicial e literal */
            break;
        lo = char_da_classe(a);
        if (a->erro != NULL)
            return frag_vazio(a);
        hi = lo;
        if (atual(a) == '-' && a->re[a->pos + 1] != ']' && a->re[a->pos + 1] != '\0') {
            a->pos++;
            hi = char_da_classe(a);
            if (a->erro != NULL)
                return frag_vazio(a);
            if (hi < lo) {
                erro(a, "faixa invalida numa classe (inicio maior que o fim)");
                return frag_vazio(a);
            }
        }
        for (c = lo; c <= hi; c++)
            classe_add(cl, c);
        primeiro = 0;
    }
    a->pos++;   /* ']' */
    if (negada)
        for (i = 0; i < 32; i++)
            cl[i] = (unsigned char) ~cl[i];
    cl[0] &= (unsigned char) ~1u;   /* '\0' nunca casa */
    return frag_classe(a, cl);
}

static Frag parse_alt(Afn *a);

static Frag parse_atomo(Afn *a)
{
    unsigned char cl[32];
    Frag f;
    int c;

    memset(cl, 0, sizeof cl);
    switch (atual(a)) {
    case '(':
        a->pos++;
        f = parse_alt(a);
        if (atual(a) != ')') {
            erro(a, "falta ')'");
            return f;
        }
        a->pos++;
        return f;
    case '[':
        a->pos++;
        return parse_classe(a);
    case '.':
        a->pos++;
        for (c = 1; c < 256; c++)
            if (c != '\n')
                classe_add(cl, c);
        return frag_classe(a, cl);
    case '*':
    case '+':
    case '?':
        erro(a, "operador *, + ou ? sem operando");
        a->pos++;
        return frag_vazio(a);
    case '\\':
        a->pos++;
        if (atual(a) == '\0') {
            erro(a, "barra invertida no fim da expressao");
            return frag_vazio(a);
        }
        classe_add(cl, escape(atual(a)));
        a->pos++;
        return frag_classe(a, cl);
    default:
        classe_add(cl, (unsigned char) atual(a));
        a->pos++;
        return frag_classe(a, cl);
    }
}

static Frag parse_repet(Afn *a)
{
    Frag f = parse_atomo(a), r;

    while (a->erro == NULL && (atual(a) == '*' || atual(a) == '+' || atual(a) == '?')) {
        char op = atual(a);
        a->pos++;
        r.ini = novo_estado(a);
        r.fim = novo_estado(a);
        ligar_vazio(a, r.ini, f.ini);
        if (op != '+')                  /* * e ?: podem pular a */
            ligar_vazio(a, r.ini, r.fim);
        ligar_vazio(a, f.fim, r.fim);
        if (op != '?')                  /* * e +: podem repetir a */
            ligar_vazio(a, f.fim, f.ini);
        f = r;
    }
    return f;
}

static int fim_de_concat(const Afn *a)
{
    return atual(a) == '\0' || atual(a) == '|' || atual(a) == ')';
}

static Frag parse_concat(Afn *a)
{
    Frag f, g;

    if (fim_de_concat(a))
        return frag_vazio(a);
    f = parse_repet(a);
    while (a->erro == NULL && !fim_de_concat(a)) {
        g = parse_repet(a);
        ligar_vazio(a, f.fim, g.ini);
        f.fim = g.fim;
    }
    return f;
}

static Frag parse_alt(Afn *a)
{
    Frag f = parse_concat(a), g, r;

    while (a->erro == NULL && atual(a) == '|') {
        a->pos++;
        g = parse_concat(a);
        r.ini = novo_estado(a);
        r.fim = novo_estado(a);
        ligar_vazio(a, r.ini, f.ini);
        ligar_vazio(a, r.ini, g.ini);
        ligar_vazio(a, f.fim, r.fim);
        ligar_vazio(a, g.fim, r.fim);
        f = r;
    }
    return f;
}

/* ------------------------------------------------------------------ */
/* AFD (construcao de subconjuntos)                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    Bitset *conj;        /* estados do AFN de cada estado do AFD */
    int *trans;          /* trans[d * 256 + c] = destino, ou -1 */
    int *final;
    int n, cap;
} Afd;

/* Fecha b pelas transicoes vazias do AFN. pilha tem a->n posicoes. */
static void fecho_vazio(const Afn *a, Bitset *b, int *pilha)
{
    int topo = 0, s, t;
    for (s = bitset_proximo(b, 0); s >= 0; s = bitset_proximo(b, s + 1))
        pilha[topo++] = s;
    while (topo > 0) {
        s = pilha[--topo];
        if (a->v[s].por_classe)
            continue;
        t = a->v[s].prox1;
        if (t >= 0 && !bitset_tem(b, t)) {
            bitset_add(b, t);
            pilha[topo++] = t;
        }
        t = a->v[s].prox2;
        if (t >= 0 && !bitset_tem(b, t)) {
            bitset_add(b, t);
            pilha[topo++] = t;
        }
    }
}

static int conj_iguais(const Bitset *x, const Bitset *y)
{
    return memcmp(x->w, y->w, (size_t) x->npalavras * sizeof(unsigned long)) == 0;
}

/* Devolve o estado do AFD com o conjunto b, criando-o se preciso
 * (b e copiado). -1 se o limite de estados foi atingido. */
static int estado_afd(Afd *d, const Bitset *b, int aceita)
{
    int i;
    for (i = 0; i < d->n; i++)
        if (conj_iguais(&d->conj[i], b))
            return i;
    if (d->n == MAX_ESTADOS_AFD)
        return -1;
    if (d->n == d->cap) {
        d->cap = d->cap ? 2 * d->cap : 16;
        d->conj = (Bitset *) xrealloc(d->conj, (size_t) d->cap * sizeof(Bitset));
        d->trans = (int *) xrealloc(d->trans, (size_t) d->cap * 256 * sizeof(int));
        d->final = (int *) xrealloc(d->final, (size_t) d->cap * sizeof(int));
    }
    bitset_iniciar(&d->conj[d->n], b->n);
    bitset_copiar(&d->conj[d->n], b);
    for (i = 0; i < 256; i++)
        d->trans[d->n * 256 + i] = -1;
    d->final[d->n] = bitset_tem(b, aceita);
    return d->n++;
}

/* Constroi o AFD; retorna 0 ou -1 (estados demais). */
static int construir_afd(const Afn *a, const Frag *f, Afd *d)
{
    Bitset b;
    int *pilha = (int *) xmalloc((size_t) a->n * sizeof(int));
    int i, c, s, destino, ok = 0;

    bitset_iniciar(&b, a->n);
    bitset_add(&b, f->ini);
    fecho_vazio(a, &b, pilha);
    estado_afd(d, &b, f->fim);

    for (i = 0; i < d->n && ok == 0; i++) {
        for (c = 1; c < 256; c++) {
            bitset_limpar(&b);
            for (s = bitset_proximo(&d->conj[i], 0); s >= 0;
                 s = bitset_proximo(&d->conj[i], s + 1))
                if (a->v[s].por_classe && classe_tem(a->v[s].classe, c))
                    bitset_add(&b, a->v[s].prox1);
            if (bitset_vazio(&b))
                continue;
            fecho_vazio(a, &b, pilha);
            destino = estado_afd(d, &b, f->fim);
            if (destino < 0) {
                ok = -1;
                break;
            }
            d->trans[i * 256 + c] = destino;
        }
    }
    bitset_liberar(&b);
    free(pilha);
    return ok;
}

static void liberar_afd(Afd *d)
{
    int i;
    for (i = 0; i < d->n; i++)
        bitset_liberar(&d->conj[i]);
    free(d->conj);
    free(d->trans);
    free(d->final);
}

/* ------------------------------------------------------------------ */
/* Emissao do codigo C                                                */
/* ------------------------------------------------------------------ */

/* Caractere como literal C legivel: 'a', '\n', ou o codigo numerico. */
static void emitir_char(FILE *f, int c)
{
    if (c == '\n')
        fputs("'\\n'", f);
    else if (c == '\t')
        fputs("'\\t'", f);
    else if (c == '\r')
        fputs("'\\r'", f);
    else if (c == '\'' || c == '\\')
        fprintf(f, "'\\%c'", c);
    else if (c >= 32 && c < 127)
        fprintf(f, "'%c'", c);
    else
        fprintf(f, "%d", c);
}

static void emitir_afd(FILE *f, const char *nome, const Afd *d)
{
    int i, c, ini = 0, destino, primeiro;

    fprintf(f, "static size_t %s(const char *s)\n{\n", nome);
    fputs("    size_t i = 0, ultimo = 0;\n    int estado = 0, c;\n\n", f);
    fputs("    for (;;) {\n", f);
    fputs("        c = (unsigned char) s[i];\n", f);
    fputs("        switch (estado) {\n", f);
    for (i = 0; i < d->n; i++) {
        fprintf(f, "        case %d:\n", i);
        if (d->final[i])
            fputs("            ultimo = i;   /* estado final */\n", f);
        primeiro = 1;
        /* agrupa caracteres consecutivos com o mesmo destino em faixas */
        for (c = 1; c < 256; c = ini + 1) {
            destino = d->trans[i * 256 + c];
            ini = c;
            if (destino < 0)
                continue;
            while (ini + 1 < 256 && d->trans[i * 256 + ini + 1] == destino)
                ini++;
            fputs(primeiro ? "            if (" : "            else if (", f);
            if (ini == c) {
                fputs("c == ", f);
                emitir_char(f, c);
            } else {
                fputs("c >= ", f);
                emitir_char(f, c);
                fputs(" && c <= ", f);
                emitir_char(f, ini);
            }
            fprintf(f, ")\n                estado = %d;\n", destino);
            primeiro = 0;
        }
        if (primeiro)
            fputs("            return ultimo;\n", f);
        else
            fputs("            else\n                return ultimo;\n            break;\n", f);
    }
    fputs("        default:\n            return ultimo;\n", f);
    fputs("        }\n        i++;\n    }\n}\n\n", f);
}

/* ------------------------------------------------------------------ */
/* Interface                                                          */
/* ------------------------------------------------------------------ */

int regex_gerar_c(FILE *f, const char *nome, const char *regex, const char **msg)
{
    Afn a;
    Afd d;
    Frag fr;
    int r = 0;

    memset(&a, 0, sizeof a);
    memset(&d, 0, sizeof d);
    a.re = regex;
    fr = parse_alt(&a);
    if (a.erro == NULL && atual(&a) == ')')
        erro(&a, "')' sem '(' correspondente");

    if (a.erro != NULL) {
        *msg = a.erro;
        r = -1;
    } else if (construir_afd(&a, &fr, &d) != 0) {
        *msg = "automato grande demais";
        r = -1;
    } else {
        emitir_afd(f, nome, &d);
    }
    liberar_afd(&d);
    free(a.v);
    return r;
}
