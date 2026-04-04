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
  {"exit", KEYWORD, "EXIT"},
  {"finish", KEYWORD, "EXIT"},
  {"niklo", KEYWORD, "EXIT"},
  {"int", KEYWORD, "INT"},
  {"count", KEYWORD, "INT"},
  {"ginti", KEYWORD, "INT"},
  {"else", KEYWORD, "ELSE"},
  {"otherwise", KEYWORD, "ELSE"},
  {"warna", KEYWORD, "ELSE"},
  {"varna", KEYWORD, "ELSE"},
  {"if", KEYWORD, "IF"},
  {"when", KEYWORD, "IF"},
  {"agar", KEYWORD, "IF"},
  {"while", KEYWORD, "WHILE"},
  {"repeat", KEYWORD, "WHILE"},
  {"jabtak", KEYWORD, "WHILE"},
  {"switch", KEYWORD, "SWITCH"},
  {"choose", KEYWORD, "SWITCH"},
  {"chuno", KEYWORD, "SWITCH"},
  {"case", KEYWORD, "CASE"},
  {"option", KEYWORD, "CASE"},
  {"mamla", KEYWORD, "CASE"},
  {"default", KEYWORD, "DEFAULT"},
  {"fallback", KEYWORD, "DEFAULT"},
  {"baki", KEYWORD, "DEFAULT"},
  {"break", KEYWORD, "BREAK"},
  {"stop", KEYWORD, "BREAK"},
  {"ruko", KEYWORD, "BREAK"},
  {"continue", KEYWORD, "CONTINUE"},
  {"skip", KEYWORD, "CONTINUE"},
  {"jaari", KEYWORD, "CONTINUE"},
  {"write", KEYWORD, "WRITE"},
  {"report", KEYWORD, "WRITE"},
  {"likho", KEYWORD, "WRITE"},
  {"dikhao", KEYWORD, "WRITE"},
  {"namaste", KEYWORD, "NAMASTE"},
  {"namste", KEYWORD, "NAMASTE"},
  {"eq", COMP, "EQ"},
  {"same", COMP, "EQ"},
  {"barabar", COMP, "EQ"},
  {"neq", COMP, "NEQ"},
  {"different", COMP, "NEQ"},
  {"alag", COMP, "NEQ"},
  {"less", COMP, "LESS"},
  {"below", COMP, "LESS"},
  {"chhota", COMP, "LESS"},
  {"greater", COMP, "GREATER"},
  {"above", COMP, "GREATER"},
  {"bada", COMP, "GREATER"},
};

static size_t line_number = 1;

static char *copy_substring(const char *source, size_t start_index, size_t length);
static char *copy_string(const char *source);
static Token make_token(TokenType type, char *value, size_t token_line);
static void append_token(Token **tokens, size_t *count, size_t *capacity, Token token);
static const KeywordMapping *find_keyword_mapping(const char *text);
static int is_identifier_start(char ch);
static int is_identifier_part(char ch);
static void skip_line_comment(const char *current, int *current_index);
static Token make_comp_token(const char *canonical_value);

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
  char *value = malloc(length + 1);

  if(value == NULL){
    fprintf(stderr, "ERROR: out of memory while copying text\n");
    exit(1);
  }

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
    Token *resized_tokens;

    *capacity *= 2;
    resized_tokens = realloc(*tokens, sizeof(Token) * (*capacity));
    if(resized_tokens == NULL){
      fprintf(stderr, "ERROR: out of memory while expanding token buffer\n");
      exit(1);
    }
    *tokens = resized_tokens;
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

static Token make_comp_token(const char *canonical_value){
  return make_token(COMP, copy_string(canonical_value), line_number);
}

Token *generate_number(char *current, int *current_index){
  int start_index = *current_index;
  size_t value_length;
  Token *token = malloc(sizeof(Token));

  if(token == NULL){
    fprintf(stderr, "ERROR: out of memory while creating number token\n");
    exit(1);
  }

  while(current[*current_index] != '\0' && isdigit((unsigned char)current[*current_index])){
    *current_index += 1;
  }

  value_length = (size_t)(*current_index - start_index);
  *token = make_token(INT, copy_substring(current, (size_t)start_index, value_length), line_number);
  return token;
}

Token *generate_keyword_or_identifier(char *current, int *current_index){
  int start_index = *current_index;
  size_t value_length;
  char *word;
  const KeywordMapping *mapping;
  Token *token;

  while(current[*current_index] != '\0' && is_identifier_part(current[*current_index])){
    *current_index += 1;
  }

  value_length = (size_t)(*current_index - start_index);
  word = copy_substring(current, (size_t)start_index, value_length);
  mapping = find_keyword_mapping(word);
  token = malloc(sizeof(Token));
  if(token == NULL){
    fprintf(stderr, "ERROR: out of memory while creating token\n");
    exit(1);
  }

  if(mapping != NULL){
    free(word);
    *token = make_token(mapping->type, copy_string(mapping->canonical_value), line_number);
  } else {
    *token = make_token(IDENTIFIER, word, line_number);
  }

  return token;
}

Token *generate_string_token(char *current, int *current_index){
  int start_index;
  size_t value_length;
  Token *token = malloc(sizeof(Token));

  if(token == NULL){
    fprintf(stderr, "ERROR: out of memory while creating string token\n");
    exit(1);
  }

  *current_index += 1;
  start_index = *current_index;
  while(current[*current_index] != '\0' && current[*current_index] != '"'){
    if(current[*current_index] == '\n'){
      fprintf(stderr, "ERROR: unterminated string on line %zu\n", line_number);
      exit(1);
    }
    *current_index += 1;
  }

  if(current[*current_index] != '"'){
    fprintf(stderr, "ERROR: unterminated string on line %zu\n", line_number);
    exit(1);
  }

  value_length = (size_t)(*current_index - start_index);
  *token = make_token(STRING, copy_substring(current, (size_t)start_index, value_length), line_number);
  *current_index += 1;
  return token;
}

Token *generate_separator_or_operator(char *current, int *current_index, TokenType type){
  char value[2];
  Token *token = malloc(sizeof(Token));

  if(token == NULL){
    fprintf(stderr, "ERROR: out of memory while creating separator/operator token\n");
    exit(1);
  }

  value[0] = current[*current_index];
  value[1] = '\0';
  *token = make_token(type, copy_string(value), line_number);
  *current_index += 1;
  return token;
}

Token *lexer(FILE *file){
  long file_length;
  char *current;
  int current_index = 0;
  size_t token_count = 0;
  size_t token_capacity = 64;
  Token *tokens = malloc(sizeof(Token) * token_capacity);

  if(tokens == NULL){
    fprintf(stderr, "ERROR: out of memory while creating token buffer\n");
    exit(1);
  }

  line_number = 1;

  fseek(file, 0, SEEK_END);
  file_length = ftell(file);
  fseek(file, 0, SEEK_SET);

  current = malloc((size_t)file_length + 1);
  if(current == NULL){
    fprintf(stderr, "ERROR: out of memory while reading source file\n");
    exit(1);
  }

  fread(current, 1, (size_t)file_length, file);
  fclose(file);
  current[file_length] = '\0';

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

      append_token(&tokens, &token_count, &token_capacity, *token);
      free(token);
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

    fprintf(stderr, "ERROR: unexpected character '%c' on line %zu\n", ch, line_number);
    exit(1);
  }

  append_token(&tokens, &token_count, &token_capacity, make_token(END_OF_TOKENS, NULL, line_number));
  free(current);
  return tokens;
}
