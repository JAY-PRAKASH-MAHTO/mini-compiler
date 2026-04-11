#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "codegeneratorf.h"
#include "diagnostics.h"
#ifdef _WIN32
#include "editor_win.h"
#endif
#include "lexerf.h"
#include "optimizerf.h"
#include "parserf.h"
#include "semanticf.h"

typedef struct {
  const char *source_path;
  const char *output_name;
  int emit_tokens;
  int dump_ast;
  int dump_symbols;
  int asm_only;
  int keep_temps;
  int no_editor;
  int show_help;
} CompilerOptions;

static int has_extension(const char *filename, const char *extension);
static void append_extension_if_missing(const char *input, const char *extension, char *output, size_t output_size);
static long current_process_id(void);
static void build_temp_filenames(char *asm_filename, size_t asm_size, char *object_filename, size_t object_size, const char *asm_extension);
static int file_exists(const char *path);
static int check_tool_command(const char *tool_command);
#ifdef _WIN32
static int prepend_tool_directory_to_path(const char *tool_path);
#endif
static int resolve_tool_command(const char *tool, char *resolved_command, size_t resolved_command_size);
static void print_usage(const char *program_name);
static int parse_options(int argc, char *argv[], CompilerOptions *options, char *error_text, size_t error_text_size);

static int has_extension(const char *filename, const char *extension){
  size_t filename_length = strlen(filename);
  size_t extension_length = strlen(extension);

  if(filename_length < extension_length){
    return 0;
  }

  return strcmp(filename + filename_length - extension_length, extension) == 0;
}

static void append_extension_if_missing(const char *input, const char *extension, char *output, size_t output_size){
  if(has_extension(input, extension)){
    snprintf(output, output_size, "%s", input);
  } else {
    snprintf(output, output_size, "%s%s", input, extension);
  }
}

static long current_process_id(void){
#ifdef _WIN32
  return (long)_getpid();
#else
  return (long)getpid();
#endif
}

static void build_temp_filenames(char *asm_filename, size_t asm_size, char *object_filename, size_t object_size, const char *asm_extension){
  long process_id = current_process_id();

  snprintf(asm_filename, asm_size, "temp_%ld%s", process_id, asm_extension);
  snprintf(object_filename, object_size, "temp_%ld.o", process_id);
}

static int file_exists(const char *path){
  FILE *file = fopen(path, "rb");

  if(file == NULL){
    return 0;
  }

  fclose(file);
  return 1;
}

static int check_tool_command(const char *tool_command){
  char command[512];

#ifdef _WIN32
  snprintf(command, sizeof(command), "\"%s\" --version >NUL 2>&1", tool_command);
#else
  snprintf(command, sizeof(command), "\"%s\" --version >/dev/null 2>&1", tool_command);
#endif

  return system(command) == 0;
}

#ifdef _WIN32
static int prepend_tool_directory_to_path(const char *tool_path){
  char directory[512];
  char new_path[4096];
  const char *last_separator = strrchr(tool_path, '\\');
  const char *existing_path = getenv("PATH");
  size_t directory_length;

  if(last_separator == NULL){
    return 0;
  }

  directory_length = (size_t)(last_separator - tool_path);
  if(directory_length == 0 || directory_length >= sizeof(directory)){
    return 0;
  }

  memcpy(directory, tool_path, directory_length);
  directory[directory_length] = '\0';

  if(existing_path != NULL && strstr(existing_path, directory) != NULL){
    return 1;
  }

  if(existing_path != NULL && existing_path[0] != '\0'){
    snprintf(new_path, sizeof(new_path), "%s;%s", directory, existing_path);
  } else {
    snprintf(new_path, sizeof(new_path), "%s", directory);
  }

  return _putenv_s("PATH", new_path) == 0;
}
#endif

static int resolve_tool_command(const char *tool, char *resolved_command, size_t resolved_command_size){
  if(resolved_command_size == 0){
    return 0;
  }

  snprintf(resolved_command, resolved_command_size, "%s", tool);
  if(check_tool_command(resolved_command)){
    return 1;
  }

#ifdef _WIN32
  if(strcmp(tool, "nasm") == 0){
    const char *local_appdata = getenv("LOCALAPPDATA");
    const char *program_files = getenv("ProgramFiles");
    const char *program_files_x86 = getenv("ProgramFiles(x86)");
    const char *candidate_roots[] = {
      local_appdata,
      program_files,
      program_files_x86,
      "C:\\msys64\\ucrt64\\bin",
      "C:\\msys64\\mingw64\\bin"
    };
    const char *candidate_suffixes[] = {
      "\\bin\\NASM\\nasm.exe",
      "\\NASM\\nasm.exe",
      "\\nasm.exe"
    };
    char candidate_path[512];

    for(size_t root_index = 0; root_index < sizeof(candidate_roots) / sizeof(candidate_roots[0]); root_index++){
      const char *root = candidate_roots[root_index];

      if(root == NULL || root[0] == '\0'){
        continue;
      }

      for(size_t suffix_index = 0; suffix_index < sizeof(candidate_suffixes) / sizeof(candidate_suffixes[0]); suffix_index++){
        snprintf(candidate_path, sizeof(candidate_path), "%s%s", root, candidate_suffixes[suffix_index]);
        if(file_exists(candidate_path) && check_tool_command(candidate_path)){
          if(prepend_tool_directory_to_path(candidate_path)){
            snprintf(resolved_command, resolved_command_size, "%s", tool);
            return check_tool_command(resolved_command);
          }
        }
      }
    }
  }
#endif

  resolved_command[0] = '\0';
  return 0;
}

static void print_usage(const char *program_name){
  printf("Usage: %s <source_file> <output_file> [options]\n", program_name);
  printf("\n");
  printf("General options:\n");
  printf("  -h, --help       Show this help text\n");
  printf("  -k, --keep-temps Keep temporary assembly/object files after a full build\n");
  printf("      --no-editor  On Windows, do not launch the GUI editor when no input is provided\n");
  printf("\n");
  printf("Debug options:\n");
  printf("      --emit-tokens   Print lexer output\n");
  printf("      --dump-ast      Print the parsed AST\n");
  printf("      --dump-symbols  Print the semantic symbol table\n");
  printf("      --asm-only      Stop after code generation and write <output_file>.asm\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s examples/hello.hin hello_demo\n", program_name);
  printf("  %s examples/full_demo.hin full_demo --keep-temps\n", program_name);
  printf("  %s examples/full_demo.hin full_demo --emit-tokens --dump-ast --dump-symbols --asm-only\n", program_name);
}

static int parse_options(int argc, char *argv[], CompilerOptions *options, char *error_text, size_t error_text_size){
  memset(options, 0, sizeof(*options));

  for(int index = 1; index < argc; index++){
    const char *argument = argv[index];

    if(strcmp(argument, "--emit-tokens") == 0){
      options->emit_tokens = 1;
    } else if(strcmp(argument, "--dump-ast") == 0){
      options->dump_ast = 1;
    } else if(strcmp(argument, "--dump-symbols") == 0){
      options->dump_symbols = 1;
    } else if(strcmp(argument, "--asm-only") == 0){
      options->asm_only = 1;
    } else if(strcmp(argument, "--keep-temps") == 0 || strcmp(argument, "-k") == 0){
      options->keep_temps = 1;
    } else if(strcmp(argument, "--no-editor") == 0){
      options->no_editor = 1;
    } else if(strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0){
      options->show_help = 1;
    } else if(argument[0] == '-'){
      snprintf(error_text, error_text_size, "Unknown option: %s", argument);
      return 0;
    } else if(options->source_path == NULL){
      options->source_path = argument;
    } else if(options->output_name == NULL){
      options->output_name = argument;
    } else {
      snprintf(error_text, error_text_size, "Unexpected extra argument: %s", argument);
      return 0;
    }
  }

  error_text[0] = '\0';
  return 1;
}

int main(int argc, char *argv[]){
  CompilerOptions options;
  ErrorList errors;
  FILE *file = NULL;
  Token *tokens = NULL;
  Node *root = NULL;
  char option_error[256];
  char asm_temp_filename[256];
  char object_temp_filename[256];
  char asm_output_filename[512];
  char native_output_filename[512];
  char nasm_tool_command[512];
  char gcc_tool_command[512];
  char assembler_command[4096];
  char linker_command[4096];
  AssemblySyntax assembly_syntax = ASM_SYNTAX_NASM;
  int use_nasm_assembler = 1;
  int compilation_succeeded = 0;
  int asm_file_generated = 0;
  int object_file_generated = 0;
  int exit_code = 1;

  error_list_init(&errors);
  option_error[0] = '\0';
  asm_temp_filename[0] = '\0';
  object_temp_filename[0] = '\0';
  asm_output_filename[0] = '\0';
  native_output_filename[0] = '\0';
  nasm_tool_command[0] = '\0';
  gcc_tool_command[0] = '\0';

  if(!parse_options(argc, argv, &options, option_error, sizeof(option_error))){
    fprintf(stderr, "ERROR: %s\n", option_error);
    print_usage(argv[0]);
    goto cleanup;
  }

  if(options.show_help){
    print_usage(argv[0]);
    exit_code = 0;
    goto cleanup;
  }

#ifdef _WIN32
  if(options.source_path == NULL){
    if(options.no_editor){
      print_usage(argv[0]);
      exit_code = 0;
      goto cleanup;
    }

    return run_editor_window();
  }
#endif

  if(options.source_path == NULL || options.output_name == NULL){
    fprintf(stderr, "ERROR: source file and output name are required.\n");
    print_usage(argv[0]);
    goto cleanup;
  }

  file = fopen(options.source_path, "r");
  if(file == NULL){
    error_list_add(&errors, "Driver", 0, "Could not open source file %s", options.source_path);
    goto finish;
  }

  tokens = lexer(file, &errors);
  fclose(file);
  file = NULL;

  if(options.emit_tokens){
    print_tokens(tokens);
  }

  root = parser(tokens, &errors);
  if(options.dump_ast){
    print_tree(root, 0, "root");
  }

  if(error_list_has_errors(&errors)){
    goto finish;
  }

  semantic_analyze(root, options.dump_symbols, &errors);
  if(error_list_has_errors(&errors)){
    goto finish;
  }

  optimize_ast(root);

  if(options.asm_only){
    append_extension_if_missing(options.output_name, ".asm", asm_output_filename, sizeof(asm_output_filename));
    if(generate_code(root, asm_output_filename) != 0){
      error_list_add(&errors, "Codegen", 0, "%s", codegenerator_last_error());
      goto finish;
    }

    printf("Generated %s\n", asm_output_filename);
    compilation_succeeded = 1;
    goto finish;
  }

  if(!resolve_tool_command("gcc", gcc_tool_command, sizeof(gcc_tool_command))){
    error_list_add(&errors, "Driver", 0, "GCC was not found in PATH. Install GCC and try again.");
    goto finish;
  }

  use_nasm_assembler = resolve_tool_command("nasm", nasm_tool_command, sizeof(nasm_tool_command));
  assembly_syntax = use_nasm_assembler ? ASM_SYNTAX_NASM : ASM_SYNTAX_GAS_INTEL;

  if(!use_nasm_assembler){
    printf("NASM was not found in PATH; using GCC's assembler fallback.\n");
  }

  build_temp_filenames(
    asm_temp_filename,
    sizeof(asm_temp_filename),
    object_temp_filename,
    sizeof(object_temp_filename),
    use_nasm_assembler ? ".asm" : ".s"
  );
  append_extension_if_missing(options.output_name, ".exe", native_output_filename, sizeof(native_output_filename));

#ifndef _WIN32
  snprintf(native_output_filename, sizeof(native_output_filename), "%s", options.output_name);
#endif

  if(generate_code_with_syntax(root, asm_temp_filename, assembly_syntax) != 0){
    error_list_add(&errors, "Codegen", 0, "%s", codegenerator_last_error());
    goto finish;
  }
  asm_file_generated = 1;

#ifdef _WIN32
  if(use_nasm_assembler){
    snprintf(
      assembler_command,
      sizeof(assembler_command),
      "%s -f win64 \"%s\" -o \"%s\"",
      nasm_tool_command,
      asm_temp_filename,
      object_temp_filename
    );
  } else {
    snprintf(
      assembler_command,
      sizeof(assembler_command),
      "%s -c \"%s\" -o \"%s\"",
      gcc_tool_command,
      asm_temp_filename,
      object_temp_filename
    );
  }
  snprintf(
    linker_command,
    sizeof(linker_command),
    "%s \"%s\" -o \"%s\"",
    gcc_tool_command,
    object_temp_filename,
    native_output_filename
  );
#else
  if(use_nasm_assembler){
    snprintf(
      assembler_command,
      sizeof(assembler_command),
      "\"%s\" -f elf64 \"%s\" -o \"%s\"",
      nasm_tool_command,
      asm_temp_filename,
      object_temp_filename
    );
  } else {
    snprintf(
      assembler_command,
      sizeof(assembler_command),
      "\"%s\" -c \"%s\" -o \"%s\"",
      gcc_tool_command,
      asm_temp_filename,
      object_temp_filename
    );
  }
  snprintf(
    linker_command,
    sizeof(linker_command),
    "\"%s\" \"%s\" -o \"%s\" -no-pie",
    gcc_tool_command,
    object_temp_filename,
    native_output_filename
  );
#endif

  if(system(assembler_command) != 0){
    error_list_add(&errors, "Assembler", 0, "Assembler failed while assembling %s", asm_temp_filename);
    goto finish;
  }
  object_file_generated = 1;

  if(system(linker_command) != 0){
    error_list_add(&errors, "Linker", 0, "GCC failed while linking %s", object_temp_filename);
    goto finish;
  }

  printf("Built %s\n", native_output_filename);
  compilation_succeeded = 1;

  if(options.keep_temps){
    printf("Retained temporary files: %s, %s\n", asm_temp_filename, object_temp_filename);
  } else {
    if(asm_file_generated){
      remove(asm_temp_filename);
      asm_file_generated = 0;
    }
    if(object_file_generated){
      remove(object_temp_filename);
      object_file_generated = 0;
    }
  }

finish:
  error_list_print(&errors);
  if(error_list_has_errors(&errors)){
    fprintf(stderr, "Compilation failed with %zu errors.\n", errors.count);
    exit_code = 1;
  } else if(compilation_succeeded){
    printf("Compilation successful.\n");
    exit_code = 0;
  }

cleanup:
  if(file != NULL){
    fclose(file);
  }
  free_ast(root);
  free_tokens(tokens);
  error_list_free(&errors);

  return exit_code;
}
