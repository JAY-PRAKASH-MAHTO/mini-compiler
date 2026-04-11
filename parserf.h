#ifndef PARSER_H_
#define PARSER_H_

#include <stddef.h>

#include "diagnostics.h"
#include "lexerf.h"

typedef struct Node{
  char *value;
  int owns_value;
  TokenType type;
  size_t line_num;
  struct Node *right;
  struct Node *left;
} Node;

Node *parser(Token *tokens, ErrorList *errors);
void print_tree(Node *node, int indent, const char *identifier);
Node *init_node(Node *node, char *value, TokenType type, size_t line_num);
void print_error(const char *error_type, size_t line_number);
void free_ast(Node *node);

#endif
