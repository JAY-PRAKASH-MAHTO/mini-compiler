#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parserf.h"

#define MAX_STACK_ENTRIES 1024
#define MAX_SYMBOLS 1024
#define MAX_SCOPES 256
#define MAX_CONTROL_CONTEXTS 256
#define MAX_SWITCH_CLAUSES 256

typedef struct {
  const char *name;
  size_t stack_slot;
} Symbol;

typedef struct {
  size_t stack_size;
  size_t symbol_count;
} ScopeFrame;

typedef struct {
  int break_label;
  int continue_label;
  size_t break_stack_size;
  size_t continue_stack_size;
} ControlContext;

typedef struct {
  Node *node;
  int label_id;
} SwitchClause;

static size_t stack_size = 0;
static Symbol symbols[MAX_SYMBOLS];
static size_t symbol_count = 0;
static ScopeFrame scope_frames[MAX_SCOPES];
static size_t scope_count = 0;
static ControlContext control_contexts[MAX_CONTROL_CONTEXTS];
static size_t control_context_count = 0;
static int next_label_id = 0;
static int next_string_label_id = 0;

static int node_value_is(const Node *node, const char *value);
static void codegen_error(const char *message);
static int create_label_id(void);
static void emit_label(FILE *file, int label_id);
static void emit_jump(FILE *file, const char *instruction, int label_id);
static void emit_push_reg(FILE *file, const char *reg);
static void emit_pop_reg(FILE *file, const char *reg);
static void emit_cleanup_to_stack_size(FILE *file, size_t target_stack_size);
static void enter_scope(void);
static void leave_scope(FILE *file);
static Symbol *find_symbol(const char *name);
static Symbol *find_symbol_in_current_scope(const char *name);
static void declare_symbol(const char *name);
static size_t stack_offset_for_slot(size_t stack_slot);
static void emit_load_stack_slot(FILE *file, size_t stack_slot, const char *reg);
static void emit_store_stack_slot(FILE *file, size_t stack_slot, const char *reg);
static size_t get_call_stack_reserve(void);
static void begin_c_call(FILE *file);
static void end_c_call(FILE *file);
static void move_first_arg_label(const char *label, FILE *file);
static void move_first_arg_from_rax(FILE *file);
static void move_second_arg_from_rax(FILE *file);
static void push_control_context(int break_label, int continue_label, size_t break_stack_size, size_t continue_stack_size);
static void pop_control_context(void);
static ControlContext *find_break_context(void);
static ControlContext *find_continue_context(void);
static void generate_expression(Node *node, FILE *file);
static void generate_condition_jump_false(Node *node, FILE *file, int false_label);
static void generate_statement_list(Node *node, FILE *file);
static void generate_block(Node *block_node, FILE *file);
static void generate_variable_declaration(Node *node, FILE *file);
static void generate_assignment(Node *node, FILE *file);
static void generate_exit_statement(Node *node, FILE *file);
static void generate_write_statement(Node *node, FILE *file);
static void generate_if_statement(Node *node, FILE *file);
static void generate_while_statement(Node *node, FILE *file);
static void generate_switch_statement(Node *node, FILE *file);
static void generate_break_statement(FILE *file);
static void generate_continue_statement(FILE *file);
static void generate_statement(Node *node, FILE *file);

static int node_value_is(const Node *node, const char *value){
  return node != NULL && strcmp(node->value, value) == 0;
}

static void codegen_error(const char *message){
  fprintf(stderr, "ERROR: %s\n", message);
  exit(1);
}

static int create_label_id(void){
  return next_label_id++;
}

static void emit_label(FILE *file, int label_id){
  fprintf(file, "label%d:\n", label_id);
}

static void emit_jump(FILE *file, const char *instruction, int label_id){
  fprintf(file, "  %s label%d\n", instruction, label_id);
}

static void emit_push_reg(FILE *file, const char *reg){
  if(stack_size >= MAX_STACK_ENTRIES){
    codegen_error("Stack tracking limit reached");
  }

  fprintf(file, "  push %s\n", reg);
  stack_size++;
}

static void emit_pop_reg(FILE *file, const char *reg){
  if(stack_size == 0){
    codegen_error("Stack underflow while generating code");
  }

  fprintf(file, "  pop %s\n", reg);
  stack_size--;
}

static void emit_cleanup_to_stack_size(FILE *file, size_t target_stack_size){
  if(stack_size < target_stack_size){
    codegen_error("Invalid cleanup target");
  }

  if(stack_size > target_stack_size){
    fprintf(file, "  add rsp, %zu\n", (stack_size - target_stack_size) * 8);
  }
}

static void enter_scope(void){
  if(scope_count >= MAX_SCOPES){
    codegen_error("Scope nesting limit reached");
  }

  scope_frames[scope_count].stack_size = stack_size;
  scope_frames[scope_count].symbol_count = symbol_count;
  scope_count++;
}

static void leave_scope(FILE *file){
  ScopeFrame frame;

  if(scope_count == 0){
    codegen_error("Tried to leave a scope that was never entered");
  }

  frame = scope_frames[scope_count - 1];
  emit_cleanup_to_stack_size(file, frame.stack_size);
  stack_size = frame.stack_size;
  symbol_count = frame.symbol_count;
  scope_count--;
}

static Symbol *find_symbol(const char *name){
  for(size_t index = symbol_count; index > 0; index--){
    Symbol *symbol = &symbols[index - 1];
    if(strcmp(symbol->name, name) == 0){
      return symbol;
    }
  }

  return NULL;
}

static Symbol *find_symbol_in_current_scope(const char *name){
  size_t start_index = 0;

  if(scope_count > 0){
    start_index = scope_frames[scope_count - 1].symbol_count;
  }

  for(size_t index = symbol_count; index > start_index; index--){
    Symbol *symbol = &symbols[index - 1];
    if(strcmp(symbol->name, name) == 0){
      return symbol;
    }
  }

  return NULL;
}

static void declare_symbol(const char *name){
  if(symbol_count >= MAX_SYMBOLS){
    codegen_error("Too many declared variables");
  }

  if(find_symbol_in_current_scope(name) != NULL){
    char error_text[256];
    snprintf(error_text, sizeof(error_text), "Variable %s is already declared in this scope", name);
    codegen_error(error_text);
  }

  symbols[symbol_count].name = name;
  symbols[symbol_count].stack_slot = stack_size;
  symbol_count++;
}

static size_t stack_offset_for_slot(size_t stack_slot){
  if(stack_size < stack_slot){
    codegen_error("Invalid stack slot lookup");
  }

  return (stack_size - stack_slot) * 8;
}

static void emit_load_stack_slot(FILE *file, size_t stack_slot, const char *reg){
  fprintf(file, "  mov %s, QWORD [rsp + %zu]\n", reg, stack_offset_for_slot(stack_slot));
}

static void emit_store_stack_slot(FILE *file, size_t stack_slot, const char *reg){
  fprintf(file, "  mov QWORD [rsp + %zu], %s\n", stack_offset_for_slot(stack_slot), reg);
}

static size_t get_call_stack_reserve(void){
#ifdef _WIN32
  return (stack_size % 2 == 0) ? 40 : 32;
#else
  return (stack_size % 2 == 0) ? 8 : 0;
#endif
}

static void begin_c_call(FILE *file){
  size_t reserve = get_call_stack_reserve();
  if(reserve > 0){
    fprintf(file, "  sub rsp, %zu\n", reserve);
  }
}

static void end_c_call(FILE *file){
  size_t reserve = get_call_stack_reserve();
  if(reserve > 0){
    fprintf(file, "  add rsp, %zu\n", reserve);
  }
}

static void move_first_arg_label(const char *label, FILE *file){
#ifdef _WIN32
  fprintf(file, "  lea rcx, [rel %s]\n", label);
#else
  fprintf(file, "  lea rdi, [rel %s]\n", label);
#endif
}

static void move_first_arg_from_rax(FILE *file){
#ifdef _WIN32
  fprintf(file, "  mov rcx, rax\n");
#else
  fprintf(file, "  mov rdi, rax\n");
#endif
}

static void move_second_arg_from_rax(FILE *file){
#ifdef _WIN32
  fprintf(file, "  mov rdx, rax\n");
#else
  fprintf(file, "  mov rsi, rax\n");
#endif
}

static void push_control_context(int break_label, int continue_label, size_t break_stack_size, size_t continue_stack_size){
  if(control_context_count >= MAX_CONTROL_CONTEXTS){
    codegen_error("Too many nested control-flow contexts");
  }

  control_contexts[control_context_count].break_label = break_label;
  control_contexts[control_context_count].continue_label = continue_label;
  control_contexts[control_context_count].break_stack_size = break_stack_size;
  control_contexts[control_context_count].continue_stack_size = continue_stack_size;
  control_context_count++;
}

static void pop_control_context(void){
  if(control_context_count == 0){
    codegen_error("Tried to pop a control-flow context that does not exist");
  }

  control_context_count--;
}

static ControlContext *find_break_context(void){
  if(control_context_count == 0){
    return NULL;
  }

  return &control_contexts[control_context_count - 1];
}

static ControlContext *find_continue_context(void){
  for(size_t index = control_context_count; index > 0; index--){
    ControlContext *context = &control_contexts[index - 1];
    if(context->continue_label >= 0){
      return context;
    }
  }

  return NULL;
}

static void generate_expression(Node *node, FILE *file){
  if(node == NULL){
    codegen_error("Missing expression");
  }

  if(node->type == INT){
    fprintf(file, "  mov rax, %s\n", node->value);
    return;
  }

  if(node->type == IDENTIFIER && node->left == NULL){
    Symbol *symbol = find_symbol(node->value);

    if(symbol == NULL){
      char error_text[256];
      snprintf(error_text, sizeof(error_text), "Variable %s is not declared", node->value);
      codegen_error(error_text);
    }

    emit_load_stack_slot(file, symbol->stack_slot, "rax");
    return;
  }

  if(node->type == OPERATOR){
    generate_expression(node->left, file);
    emit_push_reg(file, "rax");
    generate_expression(node->right, file);
    fprintf(file, "  mov r10, rax\n");
    emit_pop_reg(file, "rax");

    if(strcmp(node->value, "+") == 0){
      fprintf(file, "  add rax, r10\n");
      return;
    }
    if(strcmp(node->value, "-") == 0){
      fprintf(file, "  sub rax, r10\n");
      return;
    }
    if(strcmp(node->value, "*") == 0){
      fprintf(file, "  imul rax, r10\n");
      return;
    }
    if(strcmp(node->value, "/") == 0){
      fprintf(file, "  cqo\n");
      fprintf(file, "  idiv r10\n");
      return;
    }
    if(strcmp(node->value, "%") == 0){
      fprintf(file, "  cqo\n");
      fprintf(file, "  idiv r10\n");
      fprintf(file, "  mov rax, rdx\n");
      return;
    }

    codegen_error("Unknown operator in expression");
  }

  codegen_error("Unsupported expression");
}

static void generate_condition_jump_false(Node *node, FILE *file, int false_label){
  if(node == NULL || node->type != COMP){
    codegen_error("Invalid condition");
  }

  generate_expression(node->left, file);
  emit_push_reg(file, "rax");
  generate_expression(node->right, file);
  fprintf(file, "  mov r10, rax\n");
  emit_pop_reg(file, "rax");
  fprintf(file, "  cmp rax, r10\n");

  if(strcmp(node->value, "EQ") == 0){
    emit_jump(file, "jne", false_label);
    return;
  }
  if(strcmp(node->value, "NEQ") == 0){
    emit_jump(file, "je", false_label);
    return;
  }
  if(strcmp(node->value, "LESS") == 0){
    emit_jump(file, "jge", false_label);
    return;
  }
  if(strcmp(node->value, "GREATER") == 0){
    emit_jump(file, "jle", false_label);
    return;
  }
  if(strcmp(node->value, "LEQ") == 0){
    emit_jump(file, "jg", false_label);
    return;
  }
  if(strcmp(node->value, "GEQ") == 0){
    emit_jump(file, "jl", false_label);
    return;
  }

  codegen_error("Unknown comparator in condition");
}

static void generate_statement_list(Node *node, FILE *file){
  Node *current = node;

  while(current != NULL){
    generate_statement(current, file);
    current = current->right;
  }
}

static void generate_block(Node *block_node, FILE *file){
  if(block_node == NULL || !node_value_is(block_node, "BLOCK")){
    codegen_error("Expected a block node");
  }

  enter_scope();
  generate_statement_list(block_node->left, file);
  leave_scope(file);
}

static void generate_variable_declaration(Node *node, FILE *file){
  Node *identifier_node;

  if(node->left == NULL || node->left->type != IDENTIFIER){
    codegen_error("Malformed variable declaration");
  }

  identifier_node = node->left;
  generate_expression(identifier_node->left, file);
  emit_push_reg(file, "rax");
  declare_symbol(identifier_node->value);
}

static void generate_assignment(Node *node, FILE *file){
  Symbol *symbol = find_symbol(node->value);

  if(symbol == NULL){
    char error_text[256];
    snprintf(error_text, sizeof(error_text), "Variable %s is not declared", node->value);
    codegen_error(error_text);
  }

  generate_expression(node->left, file);
  emit_store_stack_slot(file, symbol->stack_slot, "rax");
}

static void generate_exit_statement(Node *node, FILE *file){
  generate_expression(node->left, file);
  move_first_arg_from_rax(file);
  begin_c_call(file);
  fprintf(file, "  call exit\n");
  end_c_call(file);
}

static void generate_write_statement(Node *node, FILE *file){
  Node *args_node = node->left;
  Node *value_node;

  if(args_node == NULL){
    codegen_error("Malformed write statement");
  }

  value_node = args_node->left;
  if(value_node == NULL){
    codegen_error("Missing value in write statement");
  }

  if(value_node->type == STRING){
    int string_label = next_string_label_id++;
    char label_name[32];

    snprintf(label_name, sizeof(label_name), "text%d", string_label);

    fprintf(file, "section .data\n");
    fprintf(file, "  %s db \"%s\", 0\n", label_name, value_node->value);
    fprintf(file, "section .text\n");
    move_first_arg_label(label_name, file);
    begin_c_call(file);
    fprintf(file, "  call puts\n");
    end_c_call(file);
    return;
  }

  generate_expression(value_node, file);
  move_second_arg_from_rax(file);
  move_first_arg_label("printf_format", file);
  fprintf(file, "  xor eax, eax\n");
  begin_c_call(file);
  fprintf(file, "  call printf\n");
  end_c_call(file);
}

static void generate_if_statement(Node *node, FILE *file){
  Node *if_data = node->left;
  Node *condition_node;
  Node *then_block;
  Node *else_block = NULL;
  int false_label = create_label_id();

  if(if_data == NULL){
    codegen_error("Malformed if statement");
  }

  condition_node = if_data->left;
  then_block = if_data->right;
  if(then_block != NULL){
    else_block = then_block->right;
  }

  generate_condition_jump_false(condition_node, file, false_label);
  generate_block(then_block, file);

  if(else_block != NULL){
    int end_label = create_label_id();
    emit_jump(file, "jmp", end_label);
    emit_label(file, false_label);
    generate_block(else_block, file);
    emit_label(file, end_label);
  } else {
    emit_label(file, false_label);
  }
}

static void generate_while_statement(Node *node, FILE *file){
  Node *loop_data = node->left;
  Node *condition_node;
  Node *body_block;
  int start_label = create_label_id();
  int end_label = create_label_id();
  size_t loop_stack_size = stack_size;

  if(loop_data == NULL){
    codegen_error("Malformed while statement");
  }

  condition_node = loop_data->left;
  body_block = loop_data->right;

  emit_label(file, start_label);
  generate_condition_jump_false(condition_node, file, end_label);

  push_control_context(end_label, start_label, loop_stack_size, loop_stack_size);
  generate_block(body_block, file);
  pop_control_context();

  emit_jump(file, "jmp", start_label);
  emit_label(file, end_label);
}

static void generate_switch_statement(Node *node, FILE *file){
  Node *switch_data = node->left;
  Node *expression_node;
  Node *clause_node;
  SwitchClause clauses[MAX_SWITCH_CLAUSES];
  size_t clause_count = 0;
  int default_label = -1;
  int end_label = create_label_id();
  size_t switch_stack_slot;

  if(switch_data == NULL){
    codegen_error("Malformed switch statement");
  }

  expression_node = switch_data->left;
  clause_node = switch_data->right;

  generate_expression(expression_node, file);
  emit_push_reg(file, "rax");
  switch_stack_slot = stack_size;

  while(clause_node != NULL){
    if(clause_count >= MAX_SWITCH_CLAUSES){
      codegen_error("Too many switch clauses");
    }

    clauses[clause_count].node = clause_node;
    clauses[clause_count].label_id = create_label_id();

    if(node_value_is(clause_node, "DEFAULT")){
      default_label = clauses[clause_count].label_id;
    }

    clause_count++;
    clause_node = clause_node->right;
  }

  push_control_context(end_label, -1, switch_stack_slot, 0);

  for(size_t index = 0; index < clause_count; index++){
    Node *current_clause = clauses[index].node;

    if(node_value_is(current_clause, "CASE")){
      Node *case_data = current_clause->left;
      Node *case_value;

      if(case_data == NULL){
        codegen_error("Malformed case clause");
      }

      case_value = case_data->left;
      emit_load_stack_slot(file, switch_stack_slot, "r10");
      fprintf(file, "  cmp r10, %s\n", case_value->value);
      emit_jump(file, "je", clauses[index].label_id);
    }
  }

  if(default_label >= 0){
    emit_jump(file, "jmp", default_label);
  } else {
    emit_jump(file, "jmp", end_label);
  }

  for(size_t index = 0; index < clause_count; index++){
    Node *current_clause = clauses[index].node;

    emit_label(file, clauses[index].label_id);

    if(node_value_is(current_clause, "CASE")){
      Node *case_data = current_clause->left;
      generate_block(case_data->right, file);
    } else if(node_value_is(current_clause, "DEFAULT")){
      generate_block(current_clause->left, file);
    } else {
      codegen_error("Unknown switch clause");
    }
  }

  emit_label(file, end_label);
  pop_control_context();

  emit_cleanup_to_stack_size(file, switch_stack_slot);
  if(stack_size == 0){
    codegen_error("Missing switch expression value on stack");
  }
  fprintf(file, "  add rsp, 8\n");
  stack_size--;
}

static void generate_break_statement(FILE *file){
  ControlContext *context = find_break_context();

  if(context == NULL){
    codegen_error("break can only be used inside while or switch");
  }

  emit_cleanup_to_stack_size(file, context->break_stack_size);
  emit_jump(file, "jmp", context->break_label);
}

static void generate_continue_statement(FILE *file){
  ControlContext *context = find_continue_context();

  if(context == NULL){
    codegen_error("continue can only be used inside while");
  }

  emit_cleanup_to_stack_size(file, context->continue_stack_size);
  emit_jump(file, "jmp", context->continue_label);
}

static void generate_statement(Node *node, FILE *file){
  if(node == NULL){
    return;
  }

  if(node_value_is(node, "INT")){
    generate_variable_declaration(node, file);
    return;
  }
  if(node_value_is(node, "EXIT")){
    generate_exit_statement(node, file);
    return;
  }
  if(node_value_is(node, "WRITE")){
    generate_write_statement(node, file);
    return;
  }
  if(node_value_is(node, "IF")){
    generate_if_statement(node, file);
    return;
  }
  if(node_value_is(node, "WHILE")){
    generate_while_statement(node, file);
    return;
  }
  if(node_value_is(node, "SWITCH")){
    generate_switch_statement(node, file);
    return;
  }
  if(node_value_is(node, "BREAK")){
    generate_break_statement(file);
    return;
  }
  if(node_value_is(node, "CONTINUE")){
    generate_continue_statement(file);
    return;
  }
  if(node->type == IDENTIFIER && node->left != NULL){
    generate_assignment(node, file);
    return;
  }

  codegen_error("Unsupported statement in code generator");
}

int generate_code(Node *root, char *filename){
  FILE *file = fopen(filename, "w");

  if(file == NULL){
    codegen_error("Could not open output assembly file");
  }

  stack_size = 0;
  symbol_count = 0;
  scope_count = 0;
  control_context_count = 0;
  next_label_id = 0;
  next_string_label_id = 0;

  fprintf(file, "default rel\n");
  fprintf(file, "extern printf\n");
  fprintf(file, "extern puts\n");
  fprintf(file, "extern exit\n");
  fprintf(file, "global main\n");
  fprintf(file, "section .data\n");
  fprintf(file, "  printf_format: db \"%s\", 10, 0\n", "%lld");
  fprintf(file, "section .text\n");
  fprintf(file, "main:\n");

  enter_scope();
  generate_statement_list(root->left, file);
  leave_scope(file);

  fprintf(file, "  xor eax, eax\n");
  fprintf(file, "  ret\n");

  fclose(file);
  return 0;
}
