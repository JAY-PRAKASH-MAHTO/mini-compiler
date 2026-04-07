#ifndef LEXER_H_
#define LEXER_H_

#include <stddef.h>
#include <stdio.h>

#include "diagnostics.h"

typedef enum {
  BEGINNING,
  INT,
  KEYWORD,
  SEPARATOR,
  OPERATOR,
  IDENTIFIER,
  STRING,
  COMP,
  END_OF_TOKENS,
} TokenType;

typedef struct {
  TokenType type;
  char *value;
  size_t line_num;
} Token;


void print_token(Token token);
void print_tokens(Token *tokens);
Token *lexer(FILE *file, ErrorList *errors);
void free_tokens(Token *tokens);

#endif
