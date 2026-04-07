#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap/hashmap.h"
#include "semanticf.h"

typedef struct {
  const char *name;
  size_t line_num;
  size_t scope_depth;
} SemanticSymbol;

typedef struct Scope {
  hashmap_t symbols;
  size_t scope_depth;
  struct Scope *parent;
} Scope;

typedef struct {
  SemanticSymbol **items;
  size_t count;
  size_t capacity;
} DeclarationList;

static ErrorList *semantic_error_list = NULL;
static Scope *current_scope = NULL;
static DeclarationList declared_symbols = {NULL, 0, 0};
static size_t loop_depth = 0;
static size_t switch_depth = 0;

static void *checked_malloc(size_t size);
static void *checked_realloc(void *memory, size_t size);
static int node_value_is(const Node *node, const char *value);
static void semantic_error(const char *message, size_t line_num);
static int push_declared_symbol(SemanticSymbol *symbol);
static int enter_scope(void);
static void leave_scope(void);
static SemanticSymbol *find_symbol_in_scope(Scope *scope, const char *name);
static SemanticSymbol *find_symbol(const char *name);
static SemanticSymbol *find_symbol_in_current_scope(const char *name);
static void declare_symbol(const char *name, size_t line_num);
static void analyze_expression(Node *node);
static void analyze_condition(Node *node);
static void analyze_statement_list(Node *node);
static void analyze_block(Node *block_node);
static void analyze_switch_statement(Node *node);
static void analyze_statement(Node *node);
static void dump_symbols_table(void);
static void semantic_cleanup(void);

static void *checked_malloc(size_t size){
  void *memory = malloc(size);

  if(memory == NULL){
    fprintf(stderr, "FATAL: out of memory during semantic analysis\n");
    exit(1);
  }

  return memory;
}

static void *checked_realloc(void *memory, size_t size){
  void *resized_memory = realloc(memory, size);

  if(resized_memory == NULL){
    fprintf(stderr, "FATAL: out of memory during semantic analysis\n");
    exit(1);
  }

  return resized_memory;
}

static int node_value_is(const Node *node, const char *value){
  return node != NULL && strcmp(node->value, value) == 0;
}

static void semantic_error(const char *message, size_t line_num){
  error_list_add(semantic_error_list, "Semantic", line_num, "%s", message);
}

static int push_declared_symbol(SemanticSymbol *symbol){
  if(declared_symbols.count == declared_symbols.capacity){
    size_t new_capacity = declared_symbols.capacity == 0 ? 8 : declared_symbols.capacity * 2;

    declared_symbols.items = checked_realloc(declared_symbols.items, sizeof(SemanticSymbol *) * new_capacity);
    declared_symbols.capacity = new_capacity;
  }

  declared_symbols.items[declared_symbols.count] = symbol;
  declared_symbols.count++;
  return 1;
}

static int enter_scope(void){
  Scope *scope = checked_malloc(sizeof(Scope));
  unsigned int initial_capacity = 32;

  if(hashmap_create(initial_capacity, &scope->symbols) != 0){
    free(scope);
    semantic_error("Could not create scope symbol table", 0);
    return 0;
  }

  scope->parent = current_scope;
  scope->scope_depth = current_scope == NULL ? 1 : current_scope->scope_depth + 1;
  current_scope = scope;
  return 1;
}

static void leave_scope(void){
  Scope *scope_to_free;

  if(current_scope == NULL){
    semantic_error("Tried to leave a scope that was never entered", 0);
    return;
  }

  scope_to_free = current_scope;
  current_scope = current_scope->parent;
  hashmap_destroy(&scope_to_free->symbols);
  free(scope_to_free);
}

static SemanticSymbol *find_symbol_in_scope(Scope *scope, const char *name){
  if(scope == NULL){
    return NULL;
  }

  return (SemanticSymbol *)hashmap_get(&scope->symbols, name, (hashmap_uint32_t)strlen(name));
}

static SemanticSymbol *find_symbol(const char *name){
  Scope *scope = current_scope;

  while(scope != NULL){
    SemanticSymbol *symbol = find_symbol_in_scope(scope, name);

    if(symbol != NULL){
      return symbol;
    }
    scope = scope->parent;
  }

  return NULL;
}

static SemanticSymbol *find_symbol_in_current_scope(const char *name){
  return find_symbol_in_scope(current_scope, name);
}

static void declare_symbol(const char *name, size_t line_num){
  SemanticSymbol *symbol;
  char error_text[256];

  if(current_scope == NULL){
    semantic_error("No active scope for declaration", line_num);
    return;
  }

  if(find_symbol_in_current_scope(name) != NULL){
    snprintf(error_text, sizeof(error_text), "Variable %s is already declared in this scope", name);
    semantic_error(error_text, line_num);
    return;
  }

  symbol = checked_malloc(sizeof(SemanticSymbol));
  symbol->name = name;
  symbol->line_num = line_num;
  symbol->scope_depth = current_scope->scope_depth;

  if(hashmap_put(&current_scope->symbols, name, (hashmap_uint32_t)strlen(name), symbol) != 0){
    free(symbol);
    snprintf(error_text, sizeof(error_text), "Could not store symbol %s", name);
    semantic_error(error_text, line_num);
    return;
  }

  push_declared_symbol(symbol);
}

static void analyze_expression(Node *node){
  SemanticSymbol *symbol;
  char error_text[256];

  if(node == NULL){
    semantic_error("Missing expression", 0);
    return;
  }

  if(node->type == INT){
    return;
  }

  if(node->type == STRING){
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
    return;
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
    return;
  }

  if(!enter_scope()){
    return;
  }

  analyze_statement_list(block_node->left);
  leave_scope();
}

static void analyze_switch_statement(Node *node){
  Node *switch_data;
  Node *clause_node;
  hashmap_t seen_cases;
  int cases_ready = 0;

  if(node == NULL || !node_value_is(node, "SWITCH") || node->left == NULL){
    semantic_error("Malformed chuno statement", node == NULL ? 0 : node->line_num);
    return;
  }

  switch_data = node->left;
  analyze_expression(switch_data->left);

  if(hashmap_create(16, &seen_cases) == 0){
    cases_ready = 1;
  } else {
    semantic_error("Could not create chuno-mamla lookup table", node->line_num);
  }

  switch_depth++;
  clause_node = switch_data->right;
  while(clause_node != NULL){
    if(node_value_is(clause_node, "CASE")){
      Node *case_data = clause_node->left;
      Node *case_value;

      if(case_data == NULL || case_data->left == NULL || case_data->right == NULL){
        semantic_error("Malformed case clause", clause_node->line_num);
      } else {
        case_value = case_data->left;
        if(cases_ready){
          if(hashmap_get(&seen_cases, case_value->value, (hashmap_uint32_t)strlen(case_value->value)) != NULL){
            semantic_error("Duplicate mamla value inside chuno", case_value->line_num);
          } else if(hashmap_put(&seen_cases, case_value->value, (hashmap_uint32_t)strlen(case_value->value), case_value) != 0){
            semantic_error("Could not record mamla value", case_value->line_num);
          }
        }

        analyze_block(case_data->right);
      }
    } else if(node_value_is(clause_node, "DEFAULT")){
      analyze_block(clause_node->left);
    } else {
      semantic_error("Unknown chuno clause", clause_node->line_num);
    }

    clause_node = clause_node->right;
  }
  switch_depth--;

  if(cases_ready){
    hashmap_destroy(&seen_cases);
  }
}

static void analyze_statement(Node *node){
  char error_text[256];

  if(node == NULL){
    return;
  }

  if(node_value_is(node, "INT")){
    if(node->left == NULL || node->left->type != IDENTIFIER){
      semantic_error("Malformed variable declaration", node->line_num);
      return;
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
      return;
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
      return;
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
      semantic_error("Malformed jabtak statement", node->line_num);
      return;
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
      semantic_error("ruko can only be used inside jabtak or chuno", node->line_num);
    }
    return;
  }

  if(node_value_is(node, "CONTINUE")){
    if(loop_depth == 0){
      semantic_error("jaari can only be used inside jabtak", node->line_num);
    }
    return;
  }

  semantic_error("Unsupported statement in semantic analysis", node->line_num);
}

static void dump_symbols_table(void){
  printf("Symbol table:\n");
  for(size_t index = 0; index < declared_symbols.count; index++){
    SemanticSymbol *symbol = declared_symbols.items[index];

    printf("  %s (line %zu, scope %zu)\n", symbol->name, symbol->line_num, symbol->scope_depth);
  }
}

static void semantic_cleanup(void){
  while(current_scope != NULL){
    leave_scope();
  }

  for(size_t index = 0; index < declared_symbols.count; index++){
    free(declared_symbols.items[index]);
  }

  free(declared_symbols.items);
  declared_symbols.items = NULL;
  declared_symbols.count = 0;
  declared_symbols.capacity = 0;
  loop_depth = 0;
  switch_depth = 0;
}

void semantic_analyze(Node *root, int dump_symbols, ErrorList *errors){
  semantic_error_list = errors;
  current_scope = NULL;
  declared_symbols.items = NULL;
  declared_symbols.count = 0;
  declared_symbols.capacity = 0;
  loop_depth = 0;
  switch_depth = 0;

  if(root == NULL){
    semantic_error("Missing program root", 0);
    semantic_cleanup();
    return;
  }

  if(enter_scope()){
    analyze_statement_list(root->left);
    leave_scope();
  }

  if(dump_symbols){
    dump_symbols_table();
  }

  semantic_cleanup();
}
