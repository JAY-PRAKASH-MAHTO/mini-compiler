#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostics.h"

#define DIAGNOSTIC_COLOR_RED "\x1b[31m"
#define DIAGNOSTIC_COLOR_YELLOW "\x1b[33m"
#define DIAGNOSTIC_COLOR_RESET "\x1b[0m"

static void *checked_malloc(size_t size);
static void *checked_realloc(void *memory, size_t size);
static char *duplicate_string(const char *text);

static void *checked_malloc(size_t size){
  void *memory = malloc(size);

  if(memory == NULL){
    fprintf(stderr, "FATAL: out of memory while storing diagnostics\n");
    exit(1);
  }

  return memory;
}

static void *checked_realloc(void *memory, size_t size){
  void *resized_memory = realloc(memory, size);

  if(resized_memory == NULL){
    fprintf(stderr, "FATAL: out of memory while growing diagnostics\n");
    exit(1);
  }

  return resized_memory;
}

static char *duplicate_string(const char *text){
  size_t length = strlen(text);
  char *copy = checked_malloc(length + 1);

  memcpy(copy, text, length + 1);
  return copy;
}

void error_list_init(ErrorList *list){
  list->messages = NULL;
  list->count = 0;
  list->capacity = 0;
}

void error_list_add(ErrorList *list, const char *stage, size_t line_num, const char *format, ...){
  va_list arguments;
  va_list arguments_copy;
  int message_length;
  size_t prefix_length;
  size_t final_length;
  char *formatted_message;
  char *final_message;
  const char *stage_name = stage == NULL ? "Compiler" : stage;
  char prefix[256];

  va_start(arguments, format);
  va_copy(arguments_copy, arguments);
  message_length = vsnprintf(NULL, 0, format, arguments_copy);
  va_end(arguments_copy);

  if(message_length < 0){
    va_end(arguments);
    return;
  }

  formatted_message = checked_malloc((size_t)message_length + 1);
  vsnprintf(formatted_message, (size_t)message_length + 1, format, arguments);
  va_end(arguments);

  if(line_num > 0){
    snprintf(
      prefix,
      sizeof(prefix),
      DIAGNOSTIC_COLOR_RED "[%s error]" DIAGNOSTIC_COLOR_RESET " "
      DIAGNOSTIC_COLOR_YELLOW "line %zu" DIAGNOSTIC_COLOR_RESET ": ",
      stage_name,
      line_num
    );
  } else {
    snprintf(
      prefix,
      sizeof(prefix),
      DIAGNOSTIC_COLOR_RED "[%s error]" DIAGNOSTIC_COLOR_RESET ": ",
      stage_name
    );
  }

  prefix_length = strlen(prefix);
  final_length = prefix_length + (size_t)message_length;
  final_message = checked_malloc(final_length + 1);
  memcpy(final_message, prefix, prefix_length);
  memcpy(final_message + prefix_length, formatted_message, (size_t)message_length + 1);
  free(formatted_message);

  if(list->count == list->capacity){
    size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;

    list->messages = checked_realloc(list->messages, sizeof(char *) * new_capacity);
    list->capacity = new_capacity;
  }

  list->messages[list->count] = duplicate_string(final_message);
  list->count++;
  free(final_message);
}

int error_list_has_errors(const ErrorList *list){
  return list != NULL && list->count > 0;
}

void error_list_print(const ErrorList *list){
  if(list == NULL){
    return;
  }

  for(size_t index = 0; index < list->count; index++){
    fprintf(stderr, "%s\n", list->messages[index]);
  }
}

void error_list_free(ErrorList *list){
  if(list == NULL){
    return;
  }

  for(size_t index = 0; index < list->count; index++){
    free(list->messages[index]);
  }

  free(list->messages);
  list->messages = NULL;
  list->count = 0;
  list->capacity = 0;
}
