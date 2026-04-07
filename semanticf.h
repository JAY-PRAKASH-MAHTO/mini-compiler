#ifndef SEMANTIC_H_
#define SEMANTIC_H_

#include "diagnostics.h"
#include "parserf.h"

void semantic_analyze(Node *root, int dump_symbols, ErrorList *errors);

#endif
