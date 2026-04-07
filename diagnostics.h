#ifndef DIAGNOSTICS_H_
#define DIAGNOSTICS_H_

#include <stddef.h>

typedef struct ErrorList {
  char **messages;
  size_t count;
  size_t capacity;
} ErrorList;

void error_list_init(ErrorList *list);
void error_list_add(ErrorList *list, const char *stage, size_t line_num, const char *format, ...);
int error_list_has_errors(const ErrorList *list);
void error_list_print(const ErrorList *list);
void error_list_free(ErrorList *list);

#endif
