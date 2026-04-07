#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parserf.h"

static Token *parser_tokens = NULL;
static size_t parser_index = 0;
static ErrorList *parser_error_list = NULL;

static Node *new_node(char *value, TokenType type, size_t line_num);
static Node *new_pair_node(char *value, Node *left, Node *right, size_t line_num);
static Node *new_dummy_integer_node(size_t line_num);
static Node *new_empty_block(size_t line_num);
static Token *current_token(void);
static Token *peek_token(size_t offset);
static Token *advance_token(void);
static int current_token_is(TokenType type, const char *value);
static int current_keyword_is(const char *value);
static int current_separator_is(const char *value);
static int current_operator_is(const char *value);
static int current_comparator_is_one_of(const char *first, const char *second, const char *third, const char *fourth, const char *fifth, const char *sixth);
static int token_starts_statement(const Token *token);
static void synchronize_to_statement_boundary(void);
static int expect_keyword(const char *value, const char *error_text);
static int expect_separator(const char *value, const char *error_text);
static int expect_operator(const char *value, const char *error_text);
static Node *parse_statement_list(int stop_on_switch_labels);
static Node *parse_statement(void);
static Node *parse_block(void);
static Node *parse_else_body(void);
static Node *parse_expression(void);
static Node *parse_additive_expression(void);
static Node *parse_multiplicative_expression(void);
static Node *parse_unary_expression(void);
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
static Node *parse_program_body(void);

Node *init_node(Node *node, char *value, TokenType type, size_t line_num){
  node = malloc(sizeof(Node));
  if(node == NULL){
    fprintf(stderr, "FATAL: out of memory while building syntax tree\n");
    exit(1);
  }

  node->value = value;
  node->type = type;
  node->line_num = line_num;
  node->left = NULL;
  node->right = NULL;
  return node;
}

void print_tree(Node *node, int indent, const char *identifier){
  if(node == NULL){
    return;
  }

  for(int index = 0; index < indent; index++){
    printf(" ");
  }

  printf("%s -> %s [line %zu]\n", identifier, node->value, node->line_num);
  print_tree(node->left, indent + 2, "left");
  print_tree(node->right, indent + 2, "right");
}

void print_error(const char *error_type, size_t line_number){
  error_list_add(parser_error_list, "Parser", line_number, "%s", error_type);
}

static Node *new_node(char *value, TokenType type, size_t line_num){
  return init_node(NULL, value, type, line_num);
}

static Node *new_pair_node(char *value, Node *left, Node *right, size_t line_num){
  Node *node = new_node(value, BEGINNING, line_num);

  node->left = left;
  node->right = right;
  return node;
}

static Node *new_dummy_integer_node(size_t line_num){
  return new_node("0", INT, line_num);
}

static Node *new_empty_block(size_t line_num){
  return new_node("BLOCK", BEGINNING, line_num);
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

static int current_comparator_is_one_of(const char *first, const char *second, const char *third, const char *fourth, const char *fifth, const char *sixth){
  Token *token = current_token();

  if(token->type != COMP){
    return 0;
  }

  return strcmp(token->value, first) == 0
    || strcmp(token->value, second) == 0
    || strcmp(token->value, third) == 0
    || strcmp(token->value, fourth) == 0
    || strcmp(token->value, fifth) == 0
    || strcmp(token->value, sixth) == 0;
}

static int token_starts_statement(const Token *token){
  if(token->type == IDENTIFIER){
    return 1;
  }

  if(token->type != KEYWORD){
    return 0;
  }

  return strcmp(token->value, "INT") == 0
    || strcmp(token->value, "EXIT") == 0
    || strcmp(token->value, "WRITE") == 0
    || strcmp(token->value, "IF") == 0
    || strcmp(token->value, "WHILE") == 0
    || strcmp(token->value, "SWITCH") == 0
    || strcmp(token->value, "BREAK") == 0
    || strcmp(token->value, "CONTINUE") == 0
    || strcmp(token->value, "NAMASTE") == 0;
}

static void synchronize_to_statement_boundary(void){
  size_t start_index = parser_index;

  while(current_token()->type != END_OF_TOKENS){
    if(current_separator_is(";")){
      advance_token();
      break;
    }

    if(current_separator_is("}") || current_keyword_is("CASE") || current_keyword_is("DEFAULT")){
      break;
    }

    if(parser_index != start_index && token_starts_statement(current_token())){
      break;
    }

    advance_token();
  }
}

static int expect_keyword(const char *value, const char *error_text){
  if(!current_keyword_is(value)){
    print_error(error_text, current_token()->line_num);
    return 0;
  }

  advance_token();
  return 1;
}

static int expect_separator(const char *value, const char *error_text){
  if(!current_separator_is(value)){
    print_error(error_text, current_token()->line_num);
    return 0;
  }

  advance_token();
  return 1;
}

static int expect_operator(const char *value, const char *error_text){
  if(!current_operator_is(value)){
    print_error(error_text, current_token()->line_num);
    return 0;
  }

  advance_token();
  return 1;
}

static Node *parse_primary(void){
  Token *token = current_token();

  if(current_separator_is("(")){
    Node *expression;

    advance_token();
    expression = parse_expression();
    expect_separator(")", "Expected ) after expression");
    return expression;
  }

  if(token->type == INT || token->type == IDENTIFIER){
    advance_token();
    return new_node(token->value, token->type, token->line_num);
  }

  print_error("Expected integer, identifier, or parenthesized expression", token->line_num);

  if(token->type != END_OF_TOKENS
    && !current_separator_is(")")
    && !current_separator_is("}")
    && !current_separator_is(";")
    && !current_keyword_is("CASE")
    && !current_keyword_is("DEFAULT")){
    advance_token();
  }

  return new_dummy_integer_node(token->line_num);
}

static Node *parse_unary_expression(void){
  if(current_operator_is("-")){
    Token *operator_token = advance_token();
    Node *zero_node = new_dummy_integer_node(operator_token->line_num);
    Node *operand = parse_unary_expression();
    Node *operator_node = new_node(operator_token->value, OPERATOR, operator_token->line_num);

    operator_node->left = zero_node;
    operator_node->right = operand;
    return operator_node;
  }

  return parse_primary();
}

static Node *parse_multiplicative_expression(void){
  Node *left = parse_unary_expression();

  while(current_operator_is("*") || current_operator_is("/") || current_operator_is("%")){
    Token *operator_token = advance_token();
    Node *right = parse_unary_expression();
    Node *operator_node = new_node(operator_token->value, OPERATOR, operator_token->line_num);

    operator_node->left = left;
    operator_node->right = right;
    left = operator_node;
  }

  return left;
}

static Node *parse_additive_expression(void){
  Node *left = parse_multiplicative_expression();

  while(current_operator_is("+") || current_operator_is("-")){
    Token *operator_token = advance_token();
    Node *right = parse_multiplicative_expression();
    Node *operator_node = new_node(operator_token->value, OPERATOR, operator_token->line_num);

    operator_node->left = left;
    operator_node->right = right;
    left = operator_node;
  }

  return left;
}

static Node *parse_expression(void){
  return parse_additive_expression();
}

static Node *parse_condition(void){
  Node *left = parse_expression();
  Token *comparator_token = current_token();
  Node *condition_node;

  if(!current_comparator_is_one_of("EQ", "NEQ", "LESS", "GREATER", "LEQ", "GEQ")){
    print_error("Expected comparator in condition", comparator_token->line_num);
    condition_node = new_node("EQ", COMP, comparator_token->line_num);
    condition_node->left = left;
    condition_node->right = new_dummy_integer_node(comparator_token->line_num);
    return condition_node;
  }

  advance_token();
  condition_node = new_node(comparator_token->value, COMP, comparator_token->line_num);
  condition_node->left = left;
  condition_node->right = parse_expression();
  return condition_node;
}

static Node *parse_block(void){
  size_t line_num = current_token()->line_num;
  Node *block_node;

  if(!expect_separator("{", "Expected {")){
    return new_empty_block(line_num);
  }

  block_node = new_node("BLOCK", BEGINNING, line_num);
  block_node->left = parse_statement_list(0);
  expect_separator("}", "Expected }");
  return block_node;
}

static Node *parse_else_body(void){
  if(current_keyword_is("IF")){
    Node *else_block = new_node("BLOCK", BEGINNING, current_token()->line_num);

    else_block->left = parse_if_statement();
    return else_block;
  }

  return parse_block();
}

static Node *parse_variable_declaration(void){
  Token *type_token = current_token();
  Token *identifier_token;
  Node *declaration_node;
  Node *identifier_node;

  expect_keyword("INT", "Expected ginti");

  identifier_token = current_token();
  if(identifier_token->type != IDENTIFIER){
    print_error("Expected identifier after ginti declaration", identifier_token->line_num);
    synchronize_to_statement_boundary();
    return NULL;
  }

  advance_token();
  if(!expect_operator("=", "Expected = in variable declaration")){
    synchronize_to_statement_boundary();
    return NULL;
  }

  declaration_node = new_node("INT", KEYWORD, type_token->line_num);
  identifier_node = new_node(identifier_token->value, IDENTIFIER, identifier_token->line_num);
  declaration_node->left = identifier_node;
  identifier_node->left = parse_expression();

  expect_separator(";", "Expected ; after variable declaration");
  return declaration_node;
}

static Node *parse_assignment(void){
  Token *identifier_token = advance_token();
  Node *assignment_node = new_node(identifier_token->value, IDENTIFIER, identifier_token->line_num);

  if(!expect_operator("=", "Expected = in assignment")){
    synchronize_to_statement_boundary();
    return NULL;
  }

  assignment_node->left = parse_expression();
  expect_separator(";", "Expected ; after assignment");
  return assignment_node;
}

static Node *parse_exit_statement(void){
  Token *exit_token = current_token();
  Node *exit_node;

  expect_keyword("EXIT", "Expected niklo");
  expect_separator("(", "Expected ( after niklo");

  exit_node = new_node("EXIT", KEYWORD, exit_token->line_num);
  exit_node->left = parse_expression();

  expect_separator(")", "Expected ) after niklo expression");
  expect_separator(";", "Expected ; after niklo");
  return exit_node;
}

static Node *parse_write_statement(void){
  Token *write_token = current_token();
  Node *write_node;
  Node *args_node;
  Node *first_argument;
  Node *second_argument = NULL;

  expect_keyword("WRITE", "Expected likho");
  expect_separator("(", "Expected ( after likho");

  if(current_token()->type == STRING){
    Token *string_token = advance_token();
    first_argument = new_node(string_token->value, STRING, string_token->line_num);
  } else {
    first_argument = parse_expression();
  }

  if(current_separator_is(",")){
    advance_token();
    second_argument = parse_expression();
  }

  expect_separator(")", "Expected ) after likho arguments");
  expect_separator(";", "Expected ; after likho statement");

  args_node = new_pair_node("ARGS", first_argument, second_argument, write_token->line_num);
  write_node = new_node("WRITE", KEYWORD, write_token->line_num);
  write_node->left = args_node;
  return write_node;
}

static Node *parse_if_statement(void){
  Token *if_token = current_token();
  Node *if_node;
  Node *if_data;
  Node *condition_node;
  Node *then_block;

  expect_keyword("IF", "Expected agar");
  expect_separator("(", "Expected ( after agar");
  condition_node = parse_condition();
  expect_separator(")", "Expected ) after agar condition");

  then_block = parse_block();
  if_data = new_pair_node("IFDATA", condition_node, then_block, if_token->line_num);

  if(current_keyword_is("ELSE")){
    advance_token();
    then_block->right = parse_else_body();
  }

  if_node = new_node("IF", KEYWORD, if_token->line_num);
  if_node->left = if_data;
  return if_node;
}

static Node *parse_while_statement(void){
  Token *while_token = current_token();
  Node *while_node;
  Node *while_data;
  Node *condition_node;
  Node *body_block;

  expect_keyword("WHILE", "Expected jabtak");
  expect_separator("(", "Expected ( after jabtak");
  condition_node = parse_condition();
  expect_separator(")", "Expected ) after jabtak condition");

  body_block = parse_block();
  while_data = new_pair_node("LOOPDATA", condition_node, body_block, while_token->line_num);

  while_node = new_node("WHILE", KEYWORD, while_token->line_num);
  while_node->left = while_data;
  return while_node;
}

static Node *parse_break_statement(void){
  Token *break_token = current_token();
  Node *break_node;

  expect_keyword("BREAK", "Expected ruko");
  expect_separator(";", "Expected ; after ruko");
  break_node = new_node("BREAK", KEYWORD, break_token->line_num);
  return break_node;
}

static Node *parse_continue_statement(void){
  Token *continue_token = current_token();
  Node *continue_node;

  expect_keyword("CONTINUE", "Expected jaari");
  expect_separator(";", "Expected ; after jaari");
  continue_node = new_node("CONTINUE", KEYWORD, continue_token->line_num);
  return continue_node;
}

static Node *parse_case_clause(void){
  Token *case_token = current_token();
  Token *value_token = current_token();
  Node *case_node;
  Node *case_data;
  Node *case_value;
  Node *case_block;

  expect_keyword("CASE", "Expected mamla");

  if(current_token()->type == INT){
    value_token = advance_token();
    case_value = new_node(value_token->value, INT, value_token->line_num);
  } else {
    print_error("Expected integer literal after mamla", current_token()->line_num);
    case_value = new_dummy_integer_node(current_token()->line_num);
  }

  expect_separator(":", "Expected : after mamla value");

  case_block = new_node("BLOCK", BEGINNING, case_token->line_num);
  case_block->left = parse_statement_list(1);

  case_data = new_pair_node("CASEDATA", case_value, case_block, case_token->line_num);
  case_node = new_node("CASE", KEYWORD, case_token->line_num);
  case_node->left = case_data;
  return case_node;
}

static Node *parse_default_clause(void){
  Token *default_token = current_token();
  Node *default_node;
  Node *default_block;

  expect_keyword("DEFAULT", "Expected baki");
  expect_separator(":", "Expected : after baki");

  default_block = new_node("BLOCK", BEGINNING, default_token->line_num);
  default_block->left = parse_statement_list(1);

  default_node = new_node("DEFAULT", KEYWORD, default_token->line_num);
  default_node->left = default_block;
  return default_node;
}

static Node *parse_switch_statement(void){
  Token *switch_token = current_token();
  Node *switch_node;
  Node *switch_data;
  Node *switch_expression;
  Node *first_clause = NULL;
  Node *last_clause = NULL;
  int default_seen = 0;

  expect_keyword("SWITCH", "Expected chuno");
  expect_separator("(", "Expected ( after chuno");
  switch_expression = parse_expression();
  expect_separator(")", "Expected ) after chuno expression");
  expect_separator("{", "Expected { after chuno");

  while(!current_separator_is("}") && current_token()->type != END_OF_TOKENS){
    Node *clause = NULL;

    if(current_keyword_is("CASE")){
      clause = parse_case_clause();
    } else if(current_keyword_is("DEFAULT")){
      if(default_seen){
        print_error("Only one baki clause is allowed", current_token()->line_num);
      }
      default_seen = 1;
      clause = parse_default_clause();
    } else {
      print_error("Expected mamla/baki inside chuno", current_token()->line_num);
      synchronize_to_statement_boundary();
    }

    if(clause != NULL){
      if(first_clause == NULL){
        first_clause = clause;
      } else {
        last_clause->right = clause;
      }
      last_clause = clause;
    }
  }

  expect_separator("}", "Expected } after chuno");

  switch_data = new_pair_node("SWITCHDATA", switch_expression, first_clause, switch_token->line_num);
  switch_node = new_node("SWITCH", KEYWORD, switch_token->line_num);
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
      print_error("warna without matching agar", token->line_num);
      synchronize_to_statement_boundary();
      return NULL;
    }
    if(strcmp(token->value, "CASE") == 0 || strcmp(token->value, "DEFAULT") == 0){
      print_error("mamla/baki only allowed inside chuno", token->line_num);
      synchronize_to_statement_boundary();
      return NULL;
    }
    if(strcmp(token->value, "NAMASTE") == 0){
      print_error("namaste() can only be used once at the top level", token->line_num);
      synchronize_to_statement_boundary();
      return NULL;
    }
  }

  if(token->type == IDENTIFIER && peek_token(1)->type == OPERATOR && strcmp(peek_token(1)->value, "=") == 0){
    return parse_assignment();
  }

  print_error("Unrecognized statement", token->line_num);
  synchronize_to_statement_boundary();
  return NULL;
}

static Node *parse_statement_list(int stop_on_switch_labels){
  Node *first_statement = NULL;
  Node *last_statement = NULL;

  while(current_token()->type != END_OF_TOKENS){
    Node *statement;
    size_t before_statement_index;

    if(current_separator_is("}")){
      break;
    }

    if(stop_on_switch_labels && (current_keyword_is("CASE") || current_keyword_is("DEFAULT"))){
      break;
    }

    before_statement_index = parser_index;
    statement = parse_statement();

    if(statement != NULL){
      if(first_statement == NULL){
        first_statement = statement;
      } else {
        last_statement->right = statement;
      }
      last_statement = statement;
    } else if(parser_index == before_statement_index && current_token()->type != END_OF_TOKENS){
      advance_token();
    }
  }

  return first_statement;
}

static Node *parse_program_body(void){
  if(current_keyword_is("NAMASTE")){
    Node *entry_block;

    advance_token();
    if(current_separator_is("(")){
      advance_token();
      expect_separator(")", "Expected ) after namaste(");
    }

    entry_block = parse_block();
    return entry_block->left;
  }

  return parse_statement_list(0);
}

Node *parser(Token *tokens, ErrorList *errors){
  Node *root;

  parser_tokens = tokens;
  parser_index = 0;
  parser_error_list = errors;

  root = new_node("PROGRAM", BEGINNING, current_token()->line_num);
  root->left = parse_program_body();

  while(current_token()->type != END_OF_TOKENS){
    size_t before_sync = parser_index;

    print_error("Unexpected token at end of program", current_token()->line_num);
    synchronize_to_statement_boundary();
    if(parser_index == before_sync){
      advance_token();
    }
  }

  return root;
}

void free_ast(Node *node){
  if(node == NULL){
    return;
  }

  free_ast(node->left);
  free_ast(node->right);
  free(node);
}
