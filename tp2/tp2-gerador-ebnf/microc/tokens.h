/*
 * [TP2] ARQUIVO NOVO: criado no TP2 a partir do microc.flex do TP1
 * (branch vini do repositorio trab_compiladores), onde TokenType e
 * YYSTYPE ficavam dentro do proprio .flex.
 *
 * tokens.h - Vocabulario de tokens de Micro C (TP1; livro-texto, secao 2.3.1).
 *
 * Extraido do microc.flex do TP1, onde a enumeracao ficava dentro do
 * proprio .flex. A ordem dos itens e a do livro: o analisador gerado e o
 * gerador (microc_carregar_tokens_h) dependem destes nomes.
 */
#ifndef TOKENS_H
#define TOKENS_H

typedef enum {
    /* Tokens fundamentais */
    UNDEF,          /* token indefinido (usado para reportar erros) */
    ID,             /* identificador                                */
    END_OF_FILE,    /* fim de arquivo                                */

    /* Constantes literais */
    INTEGERCONST,
    CHARCONST,
    STRINGCONST,

    /* Operadores aritmeticos */
    PLUS, MINUS, MUL, DIV, MOD,

    /* Operadores relacionais e logicos */
    EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,

    /* Simbolos de atribuicao e pontuacao */
    ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN,
    LBRACE, RBRACE, LBRACKET, RBRACKET,

    /* Palavras reservadas */
    MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
} TokenType;

/* Token entregue ao analisador sintatico (livro-texto, secao 2.3.1). */
typedef struct {
    TokenType tipo;
    char lexema[100];
    int linha;
} Token;

/* Valor semantico do token corrente, preenchido pelo scanner flex. */
typedef struct {
    char *symbol;      /* lexema para ID, INTEGERCONST, CHARCONST, STRINGCONST */
    char *error_msg;   /* mensagem de erro, usada apenas quando tipo == UNDEF   */
} YYSTYPE;

extern YYSTYPE microc_yylval;

/* Libera a tabela de strings do scanner (definida em microc.flex). */
void microc_tabela_liberar(void);

#endif
