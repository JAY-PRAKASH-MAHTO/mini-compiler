#ifndef GENERATOR_H_
#define GENERATOR_H_

#include "parserf.h"

typedef enum {
  ASM_SYNTAX_NASM = 0,
  ASM_SYNTAX_GAS_INTEL = 1
} AssemblySyntax;

int generate_code(Node *root, const char *filename);
int generate_code_with_syntax(Node *root, const char *filename, AssemblySyntax syntax);
const char *codegenerator_last_error(void);

#endif
