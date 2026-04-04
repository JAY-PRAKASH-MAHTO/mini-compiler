#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexerf.h"
#include "parserf.h"
#include "codegeneratorf.h"
#ifdef _WIN32
#include "editor_win.h"
#endif

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

int main(int argc, char *argv[]){
  #ifdef _WIN32
  if(argc == 1){
    return run_editor_window();
  }
  #endif

  if(argc != 3){
    printf("Error: correct syntax: %s <filename.unn> <output_filename>\n", argv[0]);
    return 1;
  }

  FILE *file = fopen(argv[1], "r");
  if(file == NULL){
    printf("ERROR: File not found\n");
    return 1;
  }

  Token *tokens = lexer(file);
  Node *root = parser(tokens);

  generate_code(root, "generated.asm");

  char nasm_command[1024];
  char gcc_command[1024];
  char output_filename[512];

#ifdef _WIN32
  const char *local_appdata = getenv("LOCALAPPDATA");
  char nasm_path[512];

  if(has_exe_extension(argv[2])){
    snprintf(output_filename, sizeof(output_filename), "%s", argv[2]);
  } else {
    snprintf(output_filename, sizeof(output_filename), "%s.exe", argv[2]);
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
#else
  snprintf(output_filename, sizeof(output_filename), "%s", argv[2]);
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
