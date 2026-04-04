#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semanticf.h"

#define MAX_SEMANTIC_SYMBOLS 1024
#define MAX_SEMANTIC_SCOPES 256
#define MAX_DECLARED_SYMBOLS 1024
#define MAX_SWITCH_CASES 256

typedef struct {
  const char *name;
  size_t line_num;
  size_t scope_depth;
} SemanticSymbol;

static SemanticSymbol active_symbols[MAX_SEMANTIC_SYMBOLS];
static size_t active_symbol_count = 0;
static size_t scope_symbol_counts[MAX_SEMANTIC_SCOPES];
static size_t scope_depth = 0;
static SemanticSymbol declared_symbols[MAX_DECLARED_SYMBOLS];
static size_t declared_symbol_count = 0;
static size_t loop_depth = 0;
static size_t switch_depth = 0;

static int node_value_is(const Node *node, const char *value);
static void semantic_error(const char *message, size_t line_num);
static void enter_scope(void);
static void leave_scope(void);
static SemanticSymbol *find_symbol(const char *name);
static SemanticSymbol *find_symbol_in_current_scope(const char *name);
static void remember_declared_symbol(const char *name, size_t line_num);
static void declare_symbol(const char *name, size_t line_num);
static void analyze_expression(Node *node);
static void analyze_condition(Node *node);
static void analyze_statement_list(Node *node);
static void analyze_block(Node *block_node);
static void analyze_switch_statement(Node *node);
static void analyze_statement(Node *node);
static void dump_symbols_table(void);

static int node_value_is(const Node *node, const char *value){
  return node != NULL && strcmp(node->value, value) == 0;
}

static void semantic_error(const char *message, size_t line_num){
  fprintf(stderr, "SEMANTIC ERROR: %s on line %zu\n", message, line_num);
  exit(1);
}

static void enter_scope(void){
  if(scope_depth >= MAX_SEMANTIC_SCOPES){
    semantic_error("Scope nesting limit reached", 0);
  }

  scope_symbol_counts[scope_depth] = active_symbol_count;
  scope_depth++;
}

static void leave_scope(void){
  if(scope_depth == 0){
    semantic_error("Tried to leave a scope that was never entered", 0);
  }

  scope_depth--;
  active_symbol_count = scope_symbol_counts[scope_depth];
}

static SemanticSymbol *find_symbol(const char *name){
  for(size_t index = active_symbol_count; index > 0; index--){
    SemanticSymbol *symbol = &active_symbols[index - 1];
    if(strcmp(symbol->name, name) == 0){
      return symbol;
    }
  }

  return NULL;
}

static SemanticSymbol *find_symbol_in_current_scope(const char *name){
  size_t start_index = 0;

  if(scope_depth > 0){
    start_index = scope_symbol_counts[scope_depth - 1];
  }

  for(size_t index = active_symbol_count; index > start_index; index--){
    SemanticSymbol *symbol = &active_symbols[index - 1];
    if(strcmp(symbol->name, name) == 0){
      return symbol;
    }
  }

  return NULL;
}

static void remember_declared_symbol(const char *name, size_t line_num){
  if(declared_symbol_count >= MAX_DECLARED_SYMBOLS){
    semantic_error("Symbol table dump limit reached", line_num);
  }

  declared_symbols[declared_symbol_count].name = name;
  declared_symbols[declared_symbol_count].line_num = line_num;
  declared_symbols[declared_symbol_count].scope_depth = scope_depth;
  declared_symbol_count++;
}

static void declare_symbol(const char *name, size_t line_num){
  char error_text[256];

  if(active_symbol_count >= MAX_SEMANTIC_SYMBOLS){
    semantic_error("Too many declared variables", line_num);
  }

  if(find_symbol_in_current_scope(name) != NULL){
    snprintf(error_text, sizeof(error_text), "Variable %s is already declared in this scope", name);
    semantic_error(error_text, line_num);
  }

  active_symbols[active_symbol_count].name = name;
  active_symbols[active_symbol_count].line_num = line_num;
  active_symbols[active_symbol_count].scope_depth = scope_depth;
  active_symbol_count++;

  remember_declared_symbol(name, line_num);
}

static void analyze_expression(Node *node){
  SemanticSymbol *symbol;
  char error_text[256];

  if(node == NULL){
    semantic_error("Missing expression", 0);
  }

  if(node->type == INT){
    return;
  }

  if(node->type == IDENTIFIER && node->left == NULL){
    symbol = find_symbol(node->value);
    if(symbol == NULL){
      snprintf(error_text, sizeof(error_text), "Variable %s is used before declaration", node->value);
      semantic_error(error_text, node->line_num);
    }
    return;
  }

  if(node->type == OPERATOR){
    analyze_expression(node->left);
    analyze_expression(node->right);
    return;
  }

  semantic_error("Unsupported expression in semantic analysis", node->line_num);
}

static void analyze_condition(Node *node){
  if(node == NULL || node->type != COMP){
    semantic_error("Invalid condition", node == NULL ? 0 : node->line_num);
  }

  analyze_expression(node->left);
  analyze_expression(node->right);
}

static void analyze_statement_list(Node *node){
  Node *current = node;

  while(current != NULL){
    analyze_statement(current);
    current = current->right;
  }
}

static void analyze_block(Node *block_node){
  if(block_node == NULL || !node_value_is(block_node, "BLOCK")){
    semantic_error("Expected a block node", block_node == NULL ? 0 : block_node->line_num);
  }

  enter_scope();
  analyze_statement_list(block_node->left);
  leave_scope();
}

static void analyze_switch_statement(Node *node){
  Node *switch_data;
  Node *clause_node;
  const char *seen_cases[MAX_SWITCH_CASES];
  size_t case_count = 0;

  if(node == NULL || !node_value_is(node, "SWITCH") || node->left == NULL){
    semantic_error("Malformed switch statement", node == NULL ? 0 : node->line_num);
  }

  switch_data = node->left;
  analyze_expression(switch_data->left);

  switch_depth++;
  clause_node = switch_data->right;
  while(clause_node != NULL){
    if(node_value_is(clause_node, "CASE")){
      Node *case_data = clause_node->left;
      Node *case_value;

      if(case_data == NULL || case_data->left == NULL || case_data->right == NULL){
        semantic_error("Malformed case clause", clause_node->line_num);
      }

      case_value = case_data->left;
      for(size_t index = 0; index < case_count; index++){
        if(strcmp(seen_cases[index], case_value->value) == 0){
          semantic_error("Duplicate case value inside switch", case_value->line_num);
        }
      }

      if(case_count >= MAX_SWITCH_CASES){
        semantic_error("Too many switch cases", clause_node->line_num);
      }

      seen_cases[case_count] = case_value->value;
      case_count++;
      analyze_block(case_data->right);
    } else if(node_value_is(clause_node, "DEFAULT")){
      analyze_block(clause_node->left);
    } else {
      semantic_error("Unknown switch clause", clause_node->line_num);
    }

    clause_node = clause_node->right;
  }
  switch_depth--;
}

static void analyze_statement(Node *node){
  char error_text[256];

  if(node == NULL){
    return;
  }

  if(node_value_is(node, "INT")){
    if(node->left == NULL || node->left->type != IDENTIFIER){
      semantic_error("Malformed variable declaration", node->line_num);
    }

    analyze_expression(node->left->left);
    declare_symbol(node->left->value, node->left->line_num);
    return;
  }

  if(node->type == IDENTIFIER && node->left != NULL){
    if(find_symbol(node->value) == NULL){
      snprintf(error_text, sizeof(error_text), "Variable %s is assigned before declaration", node->value);
      semantic_error(error_text, node->line_num);
    }

    analyze_expression(node->left);
    return;
  }

  if(node_value_is(node, "EXIT")){
    analyze_expression(node->left);
    return;
  }

  if(node_value_is(node, "WRITE")){
    Node *args_node = node->left;

    if(args_node == NULL || args_node->left == NULL){
      semantic_error("Malformed write statement", node->line_num);
    }

    if(args_node->left->type != STRING){
      analyze_expression(args_node->left);
    }
    if(args_node->right != NULL){
      analyze_expression(args_node->right);
    }
    return;
  }

  if(node_value_is(node, "IF")){
    Node *if_data = node->left;
    Node *then_block;
    Node *else_block = NULL;

    if(if_data == NULL || if_data->left == NULL || if_data->right == NULL){
      semantic_error("Malformed if statement", node->line_num);
    }

    then_block = if_data->right;
    if(then_block != NULL){
      else_block = then_block->right;
    }

    analyze_condition(if_data->left);
    analyze_block(then_block);
    if(else_block != NULL){
      analyze_block(else_block);
    }
    return;
  }

  if(node_value_is(node, "WHILE")){
    Node *loop_data = node->left;

    if(loop_data == NULL || loop_data->left == NULL || loop_data->right == NULL){
      semantic_error("Malformed while statement", node->line_num);
    }

    analyze_condition(loop_data->left);
    loop_depth++;
    analyze_block(loop_data->right);
    loop_depth--;
    return;
  }

  if(node_value_is(node, "SWITCH")){
    analyze_switch_statement(node);
    return;
  }

  if(node_value_is(node, "BREAK")){
    if(loop_depth == 0 && switch_depth == 0){
      semantic_error("break can only be used inside while or switch", node->line_num);
    }
    return;
  }

  if(node_value_is(node, "CONTINUE")){
    if(loop_depth == 0){
      semantic_error("continue can only be used inside while", node->line_num);
    }
    return;
  }

  semantic_error("Unsupported statement in semantic analysis", node->line_num);
}

static void dump_symbols_table(void){
  printf("Symbol table:\n");
  for(size_t index = 0; index < declared_symbol_count; index++){
    printf("  %s (line %zu, scope %zu)\n",
      declared_symbols[index].name,
      declared_symbols[index].line_num,
      declared_symbols[index].scope_depth);
  }
}

void semantic_analyze(Node *root, int dump_symbols){
  if(root == NULL){
    semantic_error("Missing program root", 0);
  }

  active_symbol_count = 0;
  scope_depth = 0;
  declared_symbol_count = 0;
  loop_depth = 0;
  switch_depth = 0;

  enter_scope();
  analyze_statement_list(root->left);
  leave_scope();

  if(dump_symbols){
    dump_symbols_table();
  }
}
