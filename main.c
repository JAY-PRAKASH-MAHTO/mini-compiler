#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegeneratorf.h"
#ifdef _WIN32
#include "editor_win.h"
#endif
#include "lexerf.h"
#include "parserf.h"
#include "semanticf.h"

typedef struct {
  const char *source_path;
  const char *output_name;
  int emit_tokens;
  int dump_ast;
  int dump_symbols;
  int asm_only;
} CompilerOptions;

static int has_exe_extension(const char *filename);
static int file_exists(const char *path);
static void print_usage(const char *program_name);
static int parse_options(int argc, char *argv[], CompilerOptions *options);

static int has_exe_extension(const char *filename){
  size_t filename_length = strlen(filename);

  return filename_length >= 4 && strcmp(filename + filename_length - 4, ".exe") == 0;
}

static int file_exists(const char *path){
  FILE *file = fopen(path, "r");

  if(file == NULL){
    return 0;
  }
  fclose(file);
  return 1;
}

static void print_usage(const char *program_name){
  printf("Usage: %s <source_file> <output_file> [options]\n", program_name);
  printf("Options:\n");
  printf("  --emit-tokens   Print lexer output\n");
  printf("  --dump-ast      Print the parsed AST\n");
  printf("  --dump-symbols  Print the semantic symbol table\n");
  printf("  --asm-only      Stop after generating generated.asm\n");
}

static int parse_options(int argc, char *argv[], CompilerOptions *options){
  if(argc < 3){
    return 0;
  }

  options->source_path = argv[1];
  options->output_name = argv[2];
  options->emit_tokens = 0;
  options->dump_ast = 0;
  options->dump_symbols = 0;
  options->asm_only = 0;

  for(int index = 3; index < argc; index++){
    if(strcmp(argv[index], "--emit-tokens") == 0){
      options->emit_tokens = 1;
    } else if(strcmp(argv[index], "--dump-ast") == 0){
      options->dump_ast = 1;
    } else if(strcmp(argv[index], "--dump-symbols") == 0){
      options->dump_symbols = 1;
    } else if(strcmp(argv[index], "--asm-only") == 0){
      options->asm_only = 1;
    } else if(strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0){
      print_usage(argv[0]);
      exit(0);
    } else {
      fprintf(stderr, "ERROR: unknown option %s\n", argv[index]);
      return 0;
    }
  }

  return 1;
}

int main(int argc, char *argv[]){
  CompilerOptions options;
  FILE *file;
  Token *tokens;
  Node *root;
  char nasm_command[1024];
  char gcc_command[1024];
  char output_filename[512];

  #ifdef _WIN32
  if(argc == 1){
    return run_editor_window();
  }
  #endif

  if(argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)){
    print_usage(argv[0]);
    return 0;
  }

  if(!parse_options(argc, argv, &options)){
    print_usage(argv[0]);
    return 1;
  }

  file = fopen(options.source_path, "r");
  if(file == NULL){
    fprintf(stderr, "ERROR: file not found: %s\n", options.source_path);
    return 1;
  }

  tokens = lexer(file);
  if(options.emit_tokens){
    print_tokens(tokens);
  }

  root = parser(tokens);
  if(options.dump_ast){
    print_tree(root, 0, "root");
  }

  semantic_analyze(root, options.dump_symbols);
  generate_code(root, "generated.asm");

  if(options.asm_only){
    printf("Generated generated.asm\n");
    return 0;
  }

  #ifdef _WIN32
  {
    const char *local_appdata = getenv("LOCALAPPDATA");
    char nasm_path[512];

    if(has_exe_extension(options.output_name)){
      snprintf(output_filename, sizeof(output_filename), "%s", options.output_name);
    } else {
      snprintf(output_filename, sizeof(output_filename), "%s.exe", options.output_name);
    }

    if(local_appdata != NULL){
      snprintf(nasm_path, sizeof(nasm_path), "%s\\bin\\NASM\\nasm.exe", local_appdata);
    } else {
      nasm_path[0] = '\0';
    }

    if(nasm_path[0] != '\0' && file_exists(nasm_path)){
      snprintf(nasm_command, sizeof(nasm_command), "\"%s\" -f win64 generated.asm -o generated.o", nasm_path);
    } else {
      snprintf(nasm_command, sizeof(nasm_command), "nasm -f win64 generated.asm -o generated.o");
    }

    snprintf(gcc_command, sizeof(gcc_command), "gcc generated.o -o \"%s\"", output_filename);
  }
  #else
  snprintf(output_filename, sizeof(output_filename), "%s", options.output_name);
  snprintf(nasm_command, sizeof(nasm_command), "nasm -f elf64 generated.asm -o generated.o");
  snprintf(gcc_command, sizeof(gcc_command), "gcc generated.o -o \"%s\" -no-pie", output_filename);
  #endif

  if(system(nasm_command) != 0){
    fprintf(stderr, "ERROR: failed to assemble generated.asm\n");
    return 1;
  }

  if(system(gcc_command) != 0){
    fprintf(stderr, "ERROR: failed to link generated.o\n");
    return 1;
  }

  printf("Built %s\n", output_filename);
  return 0;
}
