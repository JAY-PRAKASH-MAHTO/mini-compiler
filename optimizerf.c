#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "optimizerf.h"

static int node_value_is(const Node *node, const char *value);
static char *copy_text(const char *text);
static int parse_integer_literal(const char *text, long long *value);
static void replace_node_value(Node *node, char *value, int owns_value, TokenType type);
static int replace_with_integer_literal(Node *node, long long value);
static int try_compute_operator(const char *operator_value, long long left_value, long long right_value, long long *result);
static void optimize_expression(Node *node);
static void optimize_condition(Node *node);
static int evaluate_condition_constant(Node *node, int *result);
static void optimize_block(Node *block_node);
static Node *optimize_if_statement(Node *node);
static Node *optimize_while_statement(Node *node);
static void free_clause_prefix(Node *head, Node *stop);
static Node *optimize_switch_statement(Node *node);
static Node *optimize_statement(Node *node);
static int cfg_statement_list_may_fallthrough(Node *node);
static int cfg_block_may_fallthrough(Node *block_node);
static int cfg_statement_may_fallthrough(Node *node);
static void optimize_statement_list(Node **head);

static int node_value_is(const Node *node, const char *value){
  return node != NULL && strcmp(node->value, value) == 0;
}

static char *copy_text(const char *text){
  size_t length;
  char *copy;

  if(text == NULL){
    return NULL;
  }

  length = strlen(text);
  copy = malloc(length + 1);
  if(copy == NULL){
    fprintf(stderr, "FATAL: out of memory during optimization\n");
    exit(1);
  }

  memcpy(copy, text, length + 1);
  return copy;
}

static int parse_integer_literal(const char *text, long long *value){
  char *end = NULL;
  long long parsed_value;

  if(text == NULL || value == NULL){
    return 0;
  }

  parsed_value = strtoll(text, &end, 10);
  if(end == NULL || *end != '\0'){
    return 0;
  }

  *value = parsed_value;
  return 1;
}

static void replace_node_value(Node *node, char *value, int owns_value, TokenType type){
  if(node == NULL){
    return;
  }

  if(node->owns_value && node->value != NULL){
    free(node->value);
  }

  node->value = value;
  node->owns_value = owns_value;
  node->type = type;
}

static int replace_with_integer_literal(Node *node, long long value){
  char buffer[64];
  char *text;

  if(node == NULL){
    return 0;
  }

  snprintf(buffer, sizeof(buffer), "%lld", value);
  text = copy_text(buffer);
  replace_node_value(node, text, 1, INT);
  return 1;
}

static int try_compute_operator(const char *operator_value, long long left_value, long long right_value, long long *result){
#if defined(__GNUC__) || defined(__clang__)
  __int128 wide_result;
#endif

  if(operator_value == NULL || result == NULL){
    return 0;
  }

  if(strcmp(operator_value, "+") == 0){
#if defined(__GNUC__) || defined(__clang__)
    wide_result = (__int128)left_value + (__int128)right_value;
    if(wide_result < LLONG_MIN || wide_result > LLONG_MAX){
      return 0;
    }
    *result = (long long)wide_result;
    return 1;
#else
    *result = left_value + right_value;
    return 1;
#endif
  }

  if(strcmp(operator_value, "-") == 0){
#if defined(__GNUC__) || defined(__clang__)
    wide_result = (__int128)left_value - (__int128)right_value;
    if(wide_result < LLONG_MIN || wide_result > LLONG_MAX){
      return 0;
    }
    *result = (long long)wide_result;
    return 1;
#else
    *result = left_value - right_value;
    return 1;
#endif
  }

  if(strcmp(operator_value, "*") == 0){
#if defined(__GNUC__) || defined(__clang__)
    wide_result = (__int128)left_value * (__int128)right_value;
    if(wide_result < LLONG_MIN || wide_result > LLONG_MAX){
      return 0;
    }
    *result = (long long)wide_result;
    return 1;
#else
    *result = left_value * right_value;
    return 1;
#endif
  }

  if(strcmp(operator_value, "/") == 0){
    if(right_value == 0){
      return 0;
    }
    if(left_value == LLONG_MIN && right_value == -1){
      return 0;
    }
    *result = left_value / right_value;
    return 1;
  }

  if(strcmp(operator_value, "%") == 0){
    if(right_value == 0){
      return 0;
    }
    if(left_value == LLONG_MIN && right_value == -1){
      *result = 0;
      return 1;
    }
    *result = left_value % right_value;
    return 1;
  }

  return 0;
}

static void optimize_expression(Node *node){
  long long left_value;
  long long right_value;
  long long folded_value;

  if(node == NULL){
    return;
  }

  if(node->type == OPERATOR){
    optimize_expression(node->left);
    optimize_expression(node->right);

    if(node->left != NULL
      && node->right != NULL
      && node->left->type == INT
      && node->right->type == INT
      && parse_integer_literal(node->left->value, &left_value)
      && parse_integer_literal(node->right->value, &right_value)
      && try_compute_operator(node->value, left_value, right_value, &folded_value)){
      Node *left_child = node->left;
      Node *right_child = node->right;

      node->left = NULL;
      node->right = NULL;
      free_ast(left_child);
      free_ast(right_child);
      replace_with_integer_literal(node, folded_value);
    }
  }
}

static void optimize_condition(Node *node){
  if(node == NULL || node->type != COMP){
    return;
  }

  optimize_expression(node->left);
  optimize_expression(node->right);
}

static int evaluate_condition_constant(Node *node, int *result){
  long long left_value;
  long long right_value;

  if(node == NULL || node->type != COMP || result == NULL){
    return 0;
  }

  if(node->left == NULL
    || node->right == NULL
    || node->left->type != INT
    || node->right->type != INT
    || !parse_integer_literal(node->left->value, &left_value)
    || !parse_integer_literal(node->right->value, &right_value)){
    return 0;
  }

  if(strcmp(node->value, "EQ") == 0){
    *result = left_value == right_value;
    return 1;
  }
  if(strcmp(node->value, "NEQ") == 0){
    *result = left_value != right_value;
    return 1;
  }
  if(strcmp(node->value, "LESS") == 0){
    *result = left_value < right_value;
    return 1;
  }
  if(strcmp(node->value, "GREATER") == 0){
    *result = left_value > right_value;
    return 1;
  }
  if(strcmp(node->value, "LEQ") == 0){
    *result = left_value <= right_value;
    return 1;
  }
  if(strcmp(node->value, "GEQ") == 0){
    *result = left_value >= right_value;
    return 1;
  }

  return 0;
}

static void optimize_block(Node *block_node){
  if(block_node == NULL || !node_value_is(block_node, "BLOCK")){
    return;
  }

  optimize_statement_list(&block_node->left);
}

static Node *optimize_if_statement(Node *node){
  Node *if_data;
  Node *then_block;
  Node *else_block = NULL;
  Node *selected_block = NULL;
  Node *replacement;
  Node *selected_statements;
  int condition_value;

  if(node == NULL || !node_value_is(node, "IF") || node->left == NULL){
    return node;
  }

  if_data = node->left;
  then_block = if_data->right;
  if(then_block != NULL){
    else_block = then_block->right;
  }

  optimize_condition(if_data->left);
  optimize_block(then_block);
  if(else_block != NULL){
    optimize_block(else_block);
  }

  if(!evaluate_condition_constant(if_data->left, &condition_value)){
    return node;
  }

  selected_block = condition_value ? then_block : else_block;
  if(selected_block == NULL || selected_block->left == NULL){
    free_ast(node);
    return NULL;
  }

  selected_statements = selected_block->left;
  selected_block->left = NULL;

  replacement = init_node(NULL, "BLOCK", BEGINNING, selected_block->line_num);
  replacement->left = selected_statements;

  free_ast(node);
  return replacement;
}

static Node *optimize_while_statement(Node *node){
  Node *loop_data;
  int condition_value;

  if(node == NULL || !node_value_is(node, "WHILE") || node->left == NULL){
    return node;
  }

  loop_data = node->left;
  optimize_condition(loop_data->left);
  optimize_block(loop_data->right);

  if(evaluate_condition_constant(loop_data->left, &condition_value) && !condition_value){
    free_ast(node);
    return NULL;
  }

  return node;
}

static void free_clause_prefix(Node *head, Node *stop){
  Node *current = head;

  while(current != NULL && current != stop){
    Node *next = current->right;

    current->right = NULL;
    free_ast(current);
    current = next;
  }
}

static Node *optimize_switch_statement(Node *node){
  Node *switch_data;
  Node *clause_node;
  Node *default_clause = NULL;
  Node *selected_clause = NULL;
  long long switch_value;

  if(node == NULL || !node_value_is(node, "SWITCH") || node->left == NULL){
    return node;
  }

  switch_data = node->left;
  optimize_expression(switch_data->left);

  clause_node = switch_data->right;
  while(clause_node != NULL){
    if(node_value_is(clause_node, "CASE")){
      Node *case_data = clause_node->left;
      Node *case_block = case_data == NULL ? NULL : case_data->right;

      if(case_block != NULL){
        optimize_block(case_block);
      }
    } else if(node_value_is(clause_node, "DEFAULT")){
      optimize_block(clause_node->left);
      if(default_clause == NULL){
        default_clause = clause_node;
      }
    }

    clause_node = clause_node->right;
  }

  if(switch_data->left == NULL
    || switch_data->left->type != INT
    || !parse_integer_literal(switch_data->left->value, &switch_value)){
    return node;
  }

  clause_node = switch_data->right;
  while(clause_node != NULL){
    if(node_value_is(clause_node, "CASE")){
      Node *case_data = clause_node->left;
      Node *case_value = case_data == NULL ? NULL : case_data->left;
      long long case_literal;

      if(case_value != NULL
        && case_value->type == INT
        && parse_integer_literal(case_value->value, &case_literal)
        && case_literal == switch_value){
        selected_clause = clause_node;
        break;
      }
    }

    clause_node = clause_node->right;
  }

  if(selected_clause == NULL){
    selected_clause = default_clause;
  }

  if(selected_clause == NULL){
    free_ast(node);
    return NULL;
  }

  if(switch_data->right != selected_clause){
    free_clause_prefix(switch_data->right, selected_clause);
    switch_data->right = selected_clause;
  }

  return node;
}

static Node *optimize_statement(Node *node){
  if(node == NULL){
    return NULL;
  }

  if(node_value_is(node, "BLOCK")){
    optimize_block(node);
    if(node->left == NULL){
      free_ast(node);
      return NULL;
    }
    return node;
  }

  if(node_value_is(node, "INT")){
    if(node->left != NULL){
      optimize_expression(node->left->left);
    }
    return node;
  }

  if(node->type == IDENTIFIER && node->left != NULL){
    optimize_expression(node->left);
    return node;
  }

  if(node_value_is(node, "EXIT")){
    optimize_expression(node->left);
    return node;
  }

  if(node_value_is(node, "WRITE")){
    if(node->left != NULL){
      if(node->left->left != NULL && node->left->left->type != STRING){
        optimize_expression(node->left->left);
      }
      if(node->left->right != NULL){
        optimize_expression(node->left->right);
      }
    }
    return node;
  }

  if(node_value_is(node, "IF")){
    return optimize_if_statement(node);
  }

  if(node_value_is(node, "WHILE")){
    return optimize_while_statement(node);
  }

  if(node_value_is(node, "SWITCH")){
    return optimize_switch_statement(node);
  }

  return node;
}

static int cfg_statement_list_may_fallthrough(Node *node){
  Node *current = node;
  int may_fallthrough = 1;

  while(current != NULL && may_fallthrough){
    may_fallthrough = cfg_statement_may_fallthrough(current);
    current = current->right;
  }

  return may_fallthrough;
}

static int cfg_block_may_fallthrough(Node *block_node){
  if(block_node == NULL || !node_value_is(block_node, "BLOCK")){
    return 1;
  }

  return cfg_statement_list_may_fallthrough(block_node->left);
}

static int cfg_statement_may_fallthrough(Node *node){
  Node *if_data;
  Node *then_block;
  Node *else_block = NULL;
  int condition_value;

  if(node == NULL){
    return 1;
  }

  if(node_value_is(node, "EXIT") || node_value_is(node, "BREAK") || node_value_is(node, "CONTINUE")){
    return 0;
  }

  if(node_value_is(node, "BLOCK")){
    return cfg_block_may_fallthrough(node);
  }

  if(node_value_is(node, "IF") && node->left != NULL){
    if_data = node->left;
    then_block = if_data->right;
    if(then_block != NULL){
      else_block = then_block->right;
    }

    if(evaluate_condition_constant(if_data->left, &condition_value)){
      if(condition_value){
        return cfg_block_may_fallthrough(then_block);
      }
      return else_block == NULL ? 1 : cfg_block_may_fallthrough(else_block);
    }

    if(else_block == NULL){
      return 1;
    }

    return cfg_block_may_fallthrough(then_block) || cfg_block_may_fallthrough(else_block);
  }

  return 1;
}

static void optimize_statement_list(Node **head){
  Node *new_head = NULL;
  Node *tail = NULL;
  Node *current;
  int reachable = 1;

  if(head == NULL){
    return;
  }

  current = *head;
  while(current != NULL){
    Node *next = current->right;
    Node *optimized;

    current->right = NULL;

    if(!reachable){
      free_ast(current);
      current = next;
      continue;
    }

    optimized = optimize_statement(current);
    if(optimized != NULL){
      if(new_head == NULL){
        new_head = optimized;
      } else {
        tail->right = optimized;
      }
      tail = optimized;
      reachable = cfg_statement_may_fallthrough(optimized);
    }

    current = next;
  }

  *head = new_head;
}

void optimize_ast(Node *root){
  if(root == NULL){
    return;
  }

  optimize_statement_list(&root->left);
}
