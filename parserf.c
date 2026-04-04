#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parserf.h"

static Token *parser_tokens = NULL;
static size_t parser_index = 0;

static Node *new_node(char *value, TokenType type);
static Node *new_pair_node(char *value, Node *left, Node *right);
static Token *current_token(void);
static Token *peek_token(size_t offset);
static Token *advance_token(void);
static int current_token_is(TokenType type, const char *value);
static int current_keyword_is(const char *value);
static int current_separator_is(const char *value);
static int current_operator_is(const char *value);
static int is_expression_operator(Token *token);
static void expect_keyword(const char *value, const char *error_text);
static void expect_separator(const char *value, const char *error_text);
static void expect_operator(const char *value, const char *error_text);
static Node *parse_statement_list(int stop_on_switch_labels);
static Node *parse_statement(void);
static Node *parse_block(void);
static Node *parse_else_body(void);
static Node *parse_expression(void);
static Node *parse_primary(void);
static Node *parse_condition(void);
static Node *parse_variable_declaration(void);
static Node *parse_assignment(void);
static Node *parse_exit_statement(void);
static Node *parse_write_statement(void);
static Node *parse_if_statement(void);
static Node *parse_while_statement(void);
static Node *parse_break_statement(void);
static Node *parse_continue_statement(void);
static Node *parse_switch_statement(void);
static Node *parse_case_clause(void);
static Node *parse_default_clause(void);

Node *init_node(Node *node, char *value, TokenType type){
  node = malloc(sizeof(Node));
  if(node == NULL){
    fprintf(stderr, "ERROR: out of memory while building syntax tree\n");
    exit(1);
  }

  node->value = value;
  node->type = type;
  node->left = NULL;
  node->right = NULL;
  return node;
}

void print_tree(Node *node, int indent, char *identifier){
  if(node == NULL){
    return;
  }

  for(int i = 0; i < indent; i++){
    printf(" ");
  }

  printf("%s -> %s\n", identifier, node->value);
  print_tree(node->left, indent + 1, "left");
  print_tree(node->right, indent + 1, "right");
}

void print_error(char *error_type, size_t line_number){
  printf("ERROR: %s on line number: %zu\n", error_type, line_number);
  exit(1);
}

static Node *new_node(char *value, TokenType type){
  return init_node(NULL, value, type);
}

static Node *new_pair_node(char *value, Node *left, Node *right){
  Node *node = new_node(value, BEGINNING);
  node->left = left;
  node->right = right;
  return node;
}

static Token *current_token(void){
  return &parser_tokens[parser_index];
}

static Token *peek_token(size_t offset){
  return &parser_tokens[parser_index + offset];
}

static Token *advance_token(void){
  Token *token = current_token();
  if(token->type != END_OF_TOKENS){
    parser_index++;
  }
  return token;
}

static int current_token_is(TokenType type, const char *value){
  Token *token = current_token();

  if(token->type != type){
    return 0;
  }

  if(value == NULL){
    return 1;
  }

  return strcmp(token->value, value) == 0;
}

static int current_keyword_is(const char *value){
  return current_token_is(KEYWORD, value);
}

static int current_separator_is(const char *value){
  return current_token_is(SEPARATOR, value);
}

static int current_operator_is(const char *value){
  return current_token_is(OPERATOR, value);
}

static int is_expression_operator(Token *token){
  return token->type == OPERATOR && strcmp(token->value, "=") != 0;
}

static void expect_keyword(const char *value, const char *error_text){
  if(!current_keyword_is(value)){
    print_error((char *)error_text, current_token()->line_num);
  }
  advance_token();
}

static void expect_separator(const char *value, const char *error_text){
  if(!current_separator_is(value)){
    print_error((char *)error_text, current_token()->line_num);
  }
  advance_token();
}

static void expect_operator(const char *value, const char *error_text){
  if(!current_operator_is(value)){
    print_error((char *)error_text, current_token()->line_num);
  }
  advance_token();
}

static Node *parse_primary(void){
  Token *token = current_token();

  if(token->type != INT && token->type != IDENTIFIER){
    print_error("Expected integer or identifier", token->line_num);
  }

  advance_token();
  return new_node(token->value, token->type);
}

static Node *parse_expression(void){
  Node *left = parse_primary();

  while(is_expression_operator(current_token())){
    Token *operator_token = advance_token();
    Node *right = parse_primary();
    Node *operator_node = new_node(operator_token->value, OPERATOR);
    operator_node->left = left;
    operator_node->right = right;
    left = operator_node;
  }

  return left;
}

static Node *parse_condition(void){
  Node *left = parse_expression();
  Token *comparator_token = current_token();
  Node *condition_node;

  if(comparator_token->type != COMP){
    print_error("Expected comparator in condition", comparator_token->line_num);
  }

  advance_token();
  condition_node = new_node(comparator_token->value, COMP);
  condition_node->left = left;
  condition_node->right = parse_expression();
  return condition_node;
}

static Node *parse_block(void){
  Node *block_node;

  expect_separator("{", "Expected {");
  block_node = new_node("BLOCK", BEGINNING);
  block_node->left = parse_statement_list(0);
  expect_separator("}", "Expected }");
  return block_node;
}

static Node *parse_else_body(void){
  if(current_keyword_is("IF")){
    Node *else_block = new_node("BLOCK", BEGINNING);
    else_block->left = parse_if_statement();
    return else_block;
  }

  return parse_block();
}

static Node *parse_variable_declaration(void){
  Token *identifier_token;
  Node *declaration_node;
  Node *identifier_node;

  expect_keyword("INT", "Expected int");

  identifier_token = current_token();
  if(identifier_token->type != IDENTIFIER){
    print_error("Expected identifier after int", identifier_token->line_num);
  }

  advance_token();
  expect_operator("=", "Expected = in variable declaration");

  declaration_node = new_node("INT", KEYWORD);
  identifier_node = new_node(identifier_token->value, IDENTIFIER);
  declaration_node->left = identifier_node;
  identifier_node->left = parse_expression();

  expect_separator(";", "Expected ; after variable declaration");
  return declaration_node;
}

static Node *parse_assignment(void){
  Token *identifier_token = advance_token();
  Node *assignment_node = new_node(identifier_token->value, IDENTIFIER);

  expect_operator("=", "Expected = in assignment");
  assignment_node->left = parse_expression();
  expect_separator(";", "Expected ; after assignment");
  return assignment_node;
}

static Node *parse_exit_statement(void){
  Node *exit_node;

  expect_keyword("EXIT", "Expected exit");
  expect_separator("(", "Expected ( after exit");

  exit_node = new_node("EXIT", KEYWORD);
  exit_node->left = parse_expression();

  expect_separator(")", "Expected ) after exit expression");
  expect_separator(";", "Expected ; after exit");
  return exit_node;
}

static Node *parse_write_statement(void){
  Node *write_node;
  Node *args_node;
  Node *first_argument;
  Node *second_argument;

  expect_keyword("WRITE", "Expected write");
  expect_separator("(", "Expected ( after write");

  if(current_token()->type == STRING){
    Token *string_token = advance_token();
    first_argument = new_node(string_token->value, STRING);
  } else {
    first_argument = parse_expression();
  }

  expect_separator(",", "Expected , in write statement");
  second_argument = parse_expression();
  expect_separator(")", "Expected ) after write arguments");
  expect_separator(";", "Expected ; after write statement");

  args_node = new_pair_node("ARGS", first_argument, second_argument);
  write_node = new_node("WRITE", KEYWORD);
  write_node->left = args_node;
  return write_node;
}

static Node *parse_if_statement(void){
  Node *if_node;
  Node *if_data;
  Node *condition_node;
  Node *then_block;

  expect_keyword("IF", "Expected if");
  expect_separator("(", "Expected ( after if");
  condition_node = parse_condition();
  expect_separator(")", "Expected ) after if condition");

  then_block = parse_block();
  if_data = new_pair_node("IFDATA", condition_node, then_block);

  if(current_keyword_is("ELSE")){
    advance_token();
    then_block->right = parse_else_body();
  }

  if_node = new_node("IF", KEYWORD);
  if_node->left = if_data;
  return if_node;
}

static Node *parse_while_statement(void){
  Node *while_node;
  Node *while_data;
  Node *condition_node;
  Node *body_block;

  expect_keyword("WHILE", "Expected while");
  expect_separator("(", "Expected ( after while");
  condition_node = parse_condition();
  expect_separator(")", "Expected ) after while condition");

  body_block = parse_block();
  while_data = new_pair_node("LOOPDATA", condition_node, body_block);

  while_node = new_node("WHILE", KEYWORD);
  while_node->left = while_data;
  return while_node;
}

static Node *parse_break_statement(void){
  Node *break_node;

  expect_keyword("BREAK", "Expected break");
  expect_separator(";", "Expected ; after break");
  break_node = new_node("BREAK", KEYWORD);
  return break_node;
}

static Node *parse_continue_statement(void){
  Node *continue_node;

  expect_keyword("CONTINUE", "Expected continue");
  expect_separator(";", "Expected ; after continue");
  continue_node = new_node("CONTINUE", KEYWORD);
  return continue_node;
}

static Node *parse_case_clause(void){
  Token *value_token;
  Node *case_node;
  Node *case_data;
  Node *case_value;
  Node *case_block;

  expect_keyword("CASE", "Expected case");

  value_token = current_token();
  if(value_token->type != INT){
    print_error("Expected integer literal after case", value_token->line_num);
  }

  advance_token();
  expect_separator(":", "Expected : after case value");

  case_value = new_node(value_token->value, INT);
  case_block = new_node("BLOCK", BEGINNING);
  case_block->left = parse_statement_list(1);

  case_data = new_pair_node("CASEDATA", case_value, case_block);
  case_node = new_node("CASE", KEYWORD);
  case_node->left = case_data;
  return case_node;
}

static Node *parse_default_clause(void){
  Node *default_node;
  Node *default_block;

  expect_keyword("DEFAULT", "Expected default");
  expect_separator(":", "Expected : after default");

  default_block = new_node("BLOCK", BEGINNING);
  default_block->left = parse_statement_list(1);

  default_node = new_node("DEFAULT", KEYWORD);
  default_node->left = default_block;
  return default_node;
}

static Node *parse_switch_statement(void){
  Node *switch_node;
  Node *switch_data;
  Node *switch_expression;
  Node *first_clause = NULL;
  Node *last_clause = NULL;
  int default_seen = 0;

  expect_keyword("SWITCH", "Expected switch");
  expect_separator("(", "Expected ( after switch");
  switch_expression = parse_expression();
  expect_separator(")", "Expected ) after switch expression");
  expect_separator("{", "Expected { after switch");

  while(!current_separator_is("}") && current_token()->type != END_OF_TOKENS){
    Node *clause;

    if(current_keyword_is("CASE")){
      clause = parse_case_clause();
    } else if(current_keyword_is("DEFAULT")){
      if(default_seen){
        print_error("Only one default clause is allowed", current_token()->line_num);
      }
      default_seen = 1;
      clause = parse_default_clause();
    } else {
      print_error("Expected case or default inside switch", current_token()->line_num);
      clause = NULL;
    }

    if(first_clause == NULL){
      first_clause = clause;
    } else {
      last_clause->right = clause;
    }
    last_clause = clause;
  }

  expect_separator("}", "Expected } after switch");

  switch_data = new_pair_node("SWITCHDATA", switch_expression, first_clause);
  switch_node = new_node("SWITCH", KEYWORD);
  switch_node->left = switch_data;
  return switch_node;
}

static Node *parse_statement(void){
  Token *token = current_token();

  if(token->type == KEYWORD){
    if(strcmp(token->value, "INT") == 0){
      return parse_variable_declaration();
    }
    if(strcmp(token->value, "EXIT") == 0){
      return parse_exit_statement();
    }
    if(strcmp(token->value, "WRITE") == 0){
      return parse_write_statement();
    }
    if(strcmp(token->value, "IF") == 0){
      return parse_if_statement();
    }
    if(strcmp(token->value, "WHILE") == 0){
      return parse_while_statement();
    }
    if(strcmp(token->value, "SWITCH") == 0){
      return parse_switch_statement();
    }
    if(strcmp(token->value, "BREAK") == 0){
      return parse_break_statement();
    }
    if(strcmp(token->value, "CONTINUE") == 0){
      return parse_continue_statement();
    }
    if(strcmp(token->value, "ELSE") == 0){
      print_error("else without matching if", token->line_num);
    }
    if(strcmp(token->value, "CASE") == 0 || strcmp(token->value, "DEFAULT") == 0){
      print_error("case/default only allowed inside switch", token->line_num);
    }
  }

  if(token->type == IDENTIFIER && peek_token(1)->type == OPERATOR && strcmp(peek_token(1)->value, "=") == 0){
    return parse_assignment();
  }

  print_error("Unrecognized statement", token->line_num);
  return NULL;
}

static Node *parse_statement_list(int stop_on_switch_labels){
  Node *first_statement = NULL;
  Node *last_statement = NULL;

  while(current_token()->type != END_OF_TOKENS){
    Node *statement;

    if(current_separator_is("}")){
      break;
    }

    if(stop_on_switch_labels && (current_keyword_is("CASE") || current_keyword_is("DEFAULT"))){
      break;
    }

    statement = parse_statement();

    if(first_statement == NULL){
      first_statement = statement;
    } else {
      last_statement->right = statement;
    }
    last_statement = statement;
  }

  return first_statement;
}

Node *parser(Token *tokens){
  Node *root;

  parser_tokens = tokens;
  parser_index = 0;

  root = new_node("PROGRAM", BEGINNING);
  root->left = parse_statement_list(0);

  if(current_token()->type != END_OF_TOKENS){
    print_error("Unexpected token at end of program", current_token()->line_num);
  }

  return root;
}
