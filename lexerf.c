#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexerf.h"

typedef struct {
  const char *spelling;
  TokenType type;
  const char *canonical_value;
} KeywordMapping;

static const KeywordMapping keyword_mappings[] = {
  {"niklo", KEYWORD, "EXIT"},
  {"ginti", KEYWORD, "INT"},
  {"warna", KEYWORD, "ELSE"},
  {"agar", KEYWORD, "IF"},
  {"jabtak", KEYWORD, "WHILE"},
  {"chuno", KEYWORD, "SWITCH"},
  {"mamla", KEYWORD, "CASE"},
  {"baki", KEYWORD, "DEFAULT"},
  {"ruko", KEYWORD, "BREAK"},
  {"jaari", KEYWORD, "CONTINUE"},
  {"likho", KEYWORD, "WRITE"},
  {"namaste", KEYWORD, "NAMASTE"},
  {"barabar", COMP, "EQ"},
  {"alag", COMP, "NEQ"},
  {"chhota", COMP, "LESS"},
  {"bada", COMP, "GREATER"},
};

static ErrorList *lexer_errors = NULL;
static size_t line_number = 1;

static void *checked_malloc(size_t size);
static void *checked_realloc(void *memory, size_t size);
static char *copy_substring(const char *source, size_t start_index, size_t length);
static char *copy_string(const char *source);
static Token make_token(TokenType type, char *value, size_t token_line);
static void append_token(Token **tokens, size_t *count, size_t *capacity, Token token);
static const KeywordMapping *find_keyword_mapping(const char *text);
static int is_identifier_start(char ch);
static int is_identifier_part(char ch);
static void skip_line_comment(const char *current, int *current_index);
static int skip_block_comment(const char *current, int *current_index);
static Token make_comp_token(const char *canonical_value);
static Token *generate_number(char *current, int *current_index);
static Token *generate_keyword_or_identifier(char *current, int *current_index);
static Token *generate_string_token(char *current, int *current_index);
static Token *generate_separator_or_operator(char *current, int *current_index, TokenType type);
static Token *create_empty_token_stream(void);

static void *checked_malloc(size_t size){
  void *memory = malloc(size);

  if(memory == NULL){
    fprintf(stderr, "FATAL: out of memory while lexing\n");
    exit(1);
  }

  return memory;
}

static void *checked_realloc(void *memory, size_t size){
  void *resized_memory = realloc(memory, size);

  if(resized_memory == NULL){
    fprintf(stderr, "FATAL: out of memory while lexing\n");
    exit(1);
  }

  return resized_memory;
}

void print_token(Token token){
  printf("TOKEN VALUE: '%s'\n", token.value == NULL ? "(null)" : token.value);
  printf("line number: %zu", token.line_num);

  switch(token.type){
    case INT:
      printf(" TOKEN TYPE: INT\n");
      break;
    case KEYWORD:
      printf(" TOKEN TYPE: KEYWORD\n");
      break;
    case SEPARATOR:
      printf(" TOKEN TYPE: SEPARATOR\n");
      break;
    case OPERATOR:
      printf(" TOKEN TYPE: OPERATOR\n");
      break;
    case IDENTIFIER:
      printf(" TOKEN TYPE: IDENTIFIER\n");
      break;
    case STRING:
      printf(" TOKEN TYPE: STRING\n");
      break;
    case COMP:
      printf(" TOKEN TYPE: COMPARATOR\n");
      break;
    case END_OF_TOKENS:
      printf(" END OF TOKENS\n");
      break;
    case BEGINNING:
      printf(" BEGINNING\n");
      break;
  }
}

void print_tokens(Token *tokens){
  size_t index = 0;

  while(tokens[index].type != END_OF_TOKENS){
    print_token(tokens[index]);
    index++;
  }
  print_token(tokens[index]);
}

static char *copy_substring(const char *source, size_t start_index, size_t length){
  char *value = checked_malloc(length + 1);

  memcpy(value, source + start_index, length);
  value[length] = '\0';
  return value;
}

static char *copy_string(const char *source){
  return copy_substring(source, 0, strlen(source));
}

static Token make_token(TokenType type, char *value, size_t token_line){
  Token token;

  token.type = type;
  token.value = value;
  token.line_num = token_line;
  return token;
}

static void append_token(Token **tokens, size_t *count, size_t *capacity, Token token){
  if(*count + 1 >= *capacity){
    *capacity *= 2;
    *tokens = checked_realloc(*tokens, sizeof(Token) * (*capacity));
  }

  (*tokens)[*count] = token;
  (*count)++;
}

static const KeywordMapping *find_keyword_mapping(const char *text){
  size_t mapping_count = sizeof(keyword_mappings) / sizeof(keyword_mappings[0]);

  for(size_t index = 0; index < mapping_count; index++){
    if(strcmp(keyword_mappings[index].spelling, text) == 0){
      return &keyword_mappings[index];
    }
  }

  return NULL;
}

static int is_identifier_start(char ch){
  return isalpha((unsigned char)ch) || ch == '_';
}

static int is_identifier_part(char ch){
  return isalnum((unsigned char)ch) || ch == '_';
}

static void skip_line_comment(const char *current, int *current_index){
  while(current[*current_index] != '\0' && current[*current_index] != '\n'){
    *current_index += 1;
  }
}

static int skip_block_comment(const char *current, int *current_index){
  *current_index += 2;

  while(current[*current_index] != '\0'){
    if(current[*current_index] == '\n'){
      line_number++;
      *current_index += 1;
      continue;
    }

    if(current[*current_index] == '*' && current[*current_index + 1] == '/'){
      *current_index += 2;
      return 1;
    }

    *current_index += 1;
  }

  error_list_add(lexer_errors, "Lexer", line_number, "Unterminated block comment");
  return 0;
}

static Token make_comp_token(const char *canonical_value){
  return make_token(COMP, copy_string(canonical_value), line_number);
}

static Token *generate_number(char *current, int *current_index){
  int start_index = *current_index;
  size_t value_length;
  Token *token = checked_malloc(sizeof(Token));

  while(current[*current_index] != '\0' && isdigit((unsigned char)current[*current_index])){
    *current_index += 1;
  }

  value_length = (size_t)(*current_index - start_index);
  *token = make_token(INT, copy_substring(current, (size_t)start_index, value_length), line_number);
  return token;
}

static Token *generate_keyword_or_identifier(char *current, int *current_index){
  int start_index = *current_index;
  size_t value_length;
  char *word;
  const KeywordMapping *mapping;
  Token *token = checked_malloc(sizeof(Token));

  while(current[*current_index] != '\0' && is_identifier_part(current[*current_index])){
    *current_index += 1;
  }

  value_length = (size_t)(*current_index - start_index);
  word = copy_substring(current, (size_t)start_index, value_length);
  mapping = find_keyword_mapping(word);

  if(mapping != NULL){
    free(word);
    *token = make_token(mapping->type, copy_string(mapping->canonical_value), line_number);
  } else {
    *token = make_token(IDENTIFIER, word, line_number);
  }

  return token;
}

static Token *generate_string_token(char *current, int *current_index){
  int start_index;
  size_t value_length;
  Token *token = checked_malloc(sizeof(Token));

  *current_index += 1;
  start_index = *current_index;
  while(current[*current_index] != '\0' && current[*current_index] != '"'){
    if(current[*current_index] == '\n'){
      error_list_add(lexer_errors, "Lexer", line_number, "Unterminated string literal");
      free(token);
      return NULL;
    }
    *current_index += 1;
  }

  if(current[*current_index] != '"'){
    error_list_add(lexer_errors, "Lexer", line_number, "Unterminated string literal");
    free(token);
    return NULL;
  }

  value_length = (size_t)(*current_index - start_index);
  *token = make_token(STRING, copy_substring(current, (size_t)start_index, value_length), line_number);
  *current_index += 1;
  return token;
}

static Token *generate_separator_or_operator(char *current, int *current_index, TokenType type){
  char value[2];
  Token *token = checked_malloc(sizeof(Token));

  value[0] = current[*current_index];
  value[1] = '\0';
  *token = make_token(type, copy_string(value), line_number);
  *current_index += 1;
  return token;
}

static Token *create_empty_token_stream(void){
  Token *tokens = checked_malloc(sizeof(Token));

  tokens[0] = make_token(END_OF_TOKENS, NULL, line_number);
  return tokens;
}

Token *lexer(FILE *file, ErrorList *errors){
  long file_length;
  size_t bytes_read;
  char *current;
  int current_index = 0;
  size_t token_count = 0;
  size_t token_capacity = 64;
  Token *tokens = checked_malloc(sizeof(Token) * token_capacity);

  lexer_errors = errors;
  line_number = 1;

  if(fseek(file, 0, SEEK_END) != 0){
    error_list_add(errors, "Lexer", 0, "Could not seek to end of source file");
    free(tokens);
    return create_empty_token_stream();
  }

  file_length = ftell(file);
  if(file_length < 0){
    error_list_add(errors, "Lexer", 0, "Could not determine source file size");
    free(tokens);
    return create_empty_token_stream();
  }

  if(fseek(file, 0, SEEK_SET) != 0){
    error_list_add(errors, "Lexer", 0, "Could not rewind source file");
    free(tokens);
    return create_empty_token_stream();
  }

  current = checked_malloc((size_t)file_length + 1);
  bytes_read = fread(current, 1, (size_t)file_length, file);
  current[bytes_read] = '\0';

  if(ferror(file)){
    error_list_add(errors, "Lexer", 0, "Could not read the entire source file");
    free(current);
    free(tokens);
    return create_empty_token_stream();
  }

  while(current[current_index] != '\0'){
    char ch = current[current_index];

    if(ch == ' ' || ch == '\t' || ch == '\r'){
      current_index++;
      continue;
    }

    if(ch == '\n'){
      line_number++;
      current_index++;
      continue;
    }

    if(ch == '#'){
      skip_line_comment(current, &current_index);
      continue;
    }

    if(ch == '/' && current[current_index + 1] == '/'){
      current_index += 2;
      skip_line_comment(current, &current_index);
      continue;
    }

    if(ch == '/' && current[current_index + 1] == '*'){
      if(!skip_block_comment(current, &current_index)){
        break;
      }
      continue;
    }

    if(isdigit((unsigned char)ch)){
      Token *token = generate_number(current, &current_index);

      append_token(&tokens, &token_count, &token_capacity, *token);
      free(token);
      continue;
    }

    if(is_identifier_start(ch)){
      Token *token = generate_keyword_or_identifier(current, &current_index);

      append_token(&tokens, &token_count, &token_capacity, *token);
      free(token);
      continue;
    }

    if(ch == '"'){
      Token *token = generate_string_token(current, &current_index);

      if(token != NULL){
        append_token(&tokens, &token_count, &token_capacity, *token);
        free(token);
      }
      continue;
    }

    if(ch == '=' && current[current_index + 1] == '='){
      append_token(&tokens, &token_count, &token_capacity, make_comp_token("EQ"));
      current_index += 2;
      continue;
    }

    if(ch == '!' && current[current_index + 1] == '='){
      append_token(&tokens, &token_count, &token_capacity, make_comp_token("NEQ"));
      current_index += 2;
      continue;
    }

    if(ch == '<' && current[current_index + 1] == '='){
      append_token(&tokens, &token_count, &token_capacity, make_comp_token("LEQ"));
      current_index += 2;
      continue;
    }

    if(ch == '>' && current[current_index + 1] == '='){
      append_token(&tokens, &token_count, &token_capacity, make_comp_token("GEQ"));
      current_index += 2;
      continue;
    }

    if(ch == '<'){
      append_token(&tokens, &token_count, &token_capacity, make_comp_token("LESS"));
      current_index++;
      continue;
    }

    if(ch == '>'){
      append_token(&tokens, &token_count, &token_capacity, make_comp_token("GREATER"));
      current_index++;
      continue;
    }

    if(ch == ';' || ch == ':' || ch == ',' || ch == '(' || ch == ')' || ch == '{' || ch == '}'){
      Token *token = generate_separator_or_operator(current, &current_index, SEPARATOR);

      append_token(&tokens, &token_count, &token_capacity, *token);
      free(token);
      continue;
    }

    if(ch == '=' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%'){
      Token *token = generate_separator_or_operator(current, &current_index, OPERATOR);

      append_token(&tokens, &token_count, &token_capacity, *token);
      free(token);
      continue;
    }

    error_list_add(errors, "Lexer", line_number, "Unexpected character '%c'", ch);
    current_index++;
  }

  append_token(&tokens, &token_count, &token_capacity, make_token(END_OF_TOKENS, NULL, line_number));
  free(current);
  return tokens;
}

void free_tokens(Token *tokens){
  size_t index = 0;

  if(tokens == NULL){
    return;
  }

  while(tokens[index].type != END_OF_TOKENS){
    free(tokens[index].value);
    index++;
  }

  free(tokens[index].value);
  free(tokens);
}
