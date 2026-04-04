#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexerf.h"

size_t line_number = 0;

void print_token(Token token){
  printf("TOKEN VALUE: ");
  printf("'");
  for(int i = 0; token.value[i] != '\0'; i++){
    printf("%c", token.value[i]);
  }
  printf("'");
  printf("\nline number: %zu", token.line_num);

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
      printf("BEGINNING\n");
      break;
  }
}

Token *generate_number(char *current, int *current_index){
  Token *token = malloc(sizeof(Token));
  token->line_num = line_number;
  token->type = INT;
  int start_index = *current_index;
  while(current[*current_index] != '\0' && isdigit((unsigned char)current[*current_index])){
    *current_index += 1;
  }
  size_t value_length = (size_t)(*current_index - start_index);
  char *value = malloc(value_length + 1);
  memcpy(value, &current[start_index], value_length);
  value[value_length] = '\0';
  token->value = value;
  return token;
}

Token *generate_keyword_or_identifier(char *current, int *current_index){
  Token *token = malloc(sizeof(Token));
  token->line_num = line_number;
  int start_index = *current_index;
  while(current[*current_index] != '\0' && isalpha((unsigned char)current[*current_index])){
    *current_index += 1;
  }
  size_t keyword_length = (size_t)(*current_index - start_index);
  char *keyword = malloc(keyword_length + 1);
  memcpy(keyword, &current[start_index], keyword_length);
  keyword[keyword_length] = '\0';
  if(strcmp(keyword, "exit") == 0){
    token->type = KEYWORD;
    token->value = "EXIT";
  } else if(strcmp(keyword, "int") == 0){
    token->type = KEYWORD;
    token->value = "INT";
  } else if(strcmp(keyword, "else") == 0){
    token->type = KEYWORD;
    token->value = "ELSE";
  } else if(strcmp(keyword, "if") == 0){
    token->type = KEYWORD;
    token->value = "IF";
  } else if(strcmp(keyword, "while") == 0){
    token->type = KEYWORD;
    token->value = "WHILE";
  } else if(strcmp(keyword, "switch") == 0){
    token->type = KEYWORD;
    token->value = "SWITCH";
  } else if(strcmp(keyword, "case") == 0){
    token->type = KEYWORD;
    token->value = "CASE";
  } else if(strcmp(keyword, "default") == 0){
    token->type = KEYWORD;
    token->value = "DEFAULT";
  } else if(strcmp(keyword, "break") == 0){
    token->type = KEYWORD;
    token->value = "BREAK";
  } else if(strcmp(keyword, "continue") == 0){
    token->type = KEYWORD;
    token->value = "CONTINUE";
  } else if(strcmp(keyword, "write") == 0){
    token->type = KEYWORD;
    token->value = "WRITE";
  } else if(strcmp(keyword, "eq") == 0){
    token->type = COMP;
    token->value = "EQ";
  } else if(strcmp(keyword, "neq") == 0){
    token->type = COMP;
    token->value = "NEQ";
  } else if(strcmp(keyword, "less") == 0){
    token->type = COMP;
    token->value = "LESS";
  } else if(strcmp(keyword, "greater") == 0){
    token->type = COMP;
    token->value = "GREATER";
  } else {
    token->type = IDENTIFIER;
    token->value = keyword;
  }
  return token;
}

Token *generate_string_token(char *current, int *current_index){
  Token *token = malloc(sizeof(Token));
  token->line_num = line_number;
  *current_index += 1;
  int start_index = *current_index;
  while(current[*current_index] != '\0' && current[*current_index] != '"'){
    *current_index += 1;
  }
  size_t value_length = (size_t)(*current_index - start_index);
  char *value = malloc(value_length + 1);
  memcpy(value, &current[start_index], value_length);
  value[value_length] = '\0';
  token->type = STRING;
  token->value = value;
  return token;
}

Token *generate_separator_or_operator(char *current, int *current_index, TokenType type){
  Token *token = malloc(sizeof(Token));
  token->value = malloc(sizeof(char) * 2);
  token->value[0] = current[*current_index];
  token->value[1] = '\0';
  token->line_num = line_number;
  token->type = type;
  return token;
}

size_t tokens_index;

Token *lexer(FILE *file){
  int length;
  char *current = 0;

  line_number = 1;

  fseek(file, 0, SEEK_END);
  length = ftell(file);
  fseek(file, 0, SEEK_SET);

  current = malloc((size_t)length + 1);
  fread(current, 1, length, file);

  fclose(file);

  current[length] = '\0';
  int current_index = 0;

  int number_of_tokens = 12;
  int tokens_size = 0;
  Token *tokens = malloc(sizeof(Token) * number_of_tokens);
  tokens_index = 0;

  while(current[current_index] != '\0'){
    Token *token = malloc(sizeof(Token));
    tokens_size++;
    if(tokens_size > number_of_tokens){
      number_of_tokens *= 1.5;
      tokens = realloc(tokens, sizeof(Token) * number_of_tokens);
    }
    if(current[current_index] == ';'){
      token = generate_separator_or_operator(current, &current_index, SEPARATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == ':'){
      token = generate_separator_or_operator(current, &current_index, SEPARATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == ','){
      token = generate_separator_or_operator(current, &current_index, SEPARATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '('){
      token = generate_separator_or_operator(current, &current_index, SEPARATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == ')'){
      token = generate_separator_or_operator(current, &current_index, SEPARATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '{'){
      token = generate_separator_or_operator(current, &current_index, SEPARATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '}'){
      token = generate_separator_or_operator(current, &current_index, SEPARATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '='){
      token = generate_separator_or_operator(current, &current_index, OPERATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '+'){
      token = generate_separator_or_operator(current, &current_index, OPERATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '-'){
      token = generate_separator_or_operator(current, &current_index, OPERATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '*'){
      token = generate_separator_or_operator(current, &current_index, OPERATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '/'){
      token = generate_separator_or_operator(current, &current_index, OPERATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '%'){
      token = generate_separator_or_operator(current, &current_index, OPERATOR);
      tokens[tokens_index] = *token;
      tokens_index++;
    } else if(current[current_index] == '"'){
      token = generate_string_token(current, &current_index);
      tokens[tokens_index] = *token;
      tokens_index++;
    }else if(isdigit(current[current_index])){
      token = generate_number(current, &current_index); 
      tokens[tokens_index] = *token;
      tokens_index++;
      current_index--;
    } else if(isalpha(current[current_index])){
      token = generate_keyword_or_identifier(current, &current_index);
      tokens[tokens_index] = *token;
      tokens_index++;
      current_index--;
    } else if(current[current_index] == '\n'){
      line_number += 1;
    } 
    free(token);
    current_index++;
  }
  tokens[tokens_index].value = NULL;
  tokens[tokens_index].type = END_OF_TOKENS;
  tokens[tokens_index].line_num = line_number;
  return tokens;
}


