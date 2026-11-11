/*
 * [TP2] ARQUIVO NOVO: nao existia no codigo-base (ver regex.c).
 *
 * regex.h - Expressoes regulares de TOKENS convertidas em codigo C
 * (usado por gen_casamento_regex, em codegen.c).
 *
 * Sintaxe aceita: concatenacao, |, *, +, ?, parenteses, classes [a-z],
 * [^...], o ponto (qualquer caractere exceto \n) e escapes \n, \t, \r,
 * \\, \/ e \<qualquer> (o proprio caractere, ex.: \. \* \[).
 */
#ifndef REGEX_H
#define REGEX_H

#include <stdio.h>

/* Emite em f a funcao
 *     static size_t <nome>(const char *s)
 * que devolve o comprimento do MAIOR prefixo de s reconhecido pela
 * expressao regular (0 se nenhum prefixo nao vazio for reconhecido).
 * Retorna 0 em caso de sucesso; se a expressao for invalida, nada e
 * emitido, *erro aponta para a mensagem e o retorno e -1. */
int regex_gerar_c(FILE *f, const char *nome, const char *regex, const char **erro);

#endif
