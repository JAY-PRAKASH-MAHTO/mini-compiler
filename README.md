# BhasaCore

[![CI](https://github.com/JAY-PRAKASH-MAHTO/mini-compiler/actions/workflows/ci.yml/badge.svg)](https://github.com/JAY-PRAKASH-MAHTO/mini-compiler/actions/workflows/ci.yml)

BhasaCore is an end-to-end compiler written in C for a Hindi-flavored domain-specific language. It reads `.hin` source files, performs lexical analysis, parsing, semantic checking, assembly generation, and produces native executables through GCC.

```text
source.hin
  -> lexer
  -> parser / AST
  -> semantic analyzer
  -> optimizer
  -> x86-64 assembly generator
  -> object file
  -> native executable
```

## What BhasaCore Does

- Compiles `.hin` programs into native executables
- Exposes the full compiler pipeline for learning and debugging
- Supports variables, arithmetic, conditions, loops, `chuno` blocks, and output
- Performs AST-level optimization before code generation
- Includes a Windows editor for writing, compiling, and running programs
- Falls back to GCC's assembler when NASM is not available

## Complete Keyword Reference

| Category | Keyword / Token | Equivalent / Description |
| --- | --- | --- |
| Program Entry | `namaste` | Top-level program entry written as `namaste() { ... }` |
| Type | `ginti` | Integer variable declaration |
| Control Flow | `agar` | Conditional branch |
| Control Flow | `warna` | Alternative branch |
| Control Flow | `jabtak` | Iterative loop |
| Control Flow | `chuno` | Multi-way branch selector |
| Control Flow | `mamla` | Case label inside `chuno` |
| Control Flow | `baki` | Default case inside `chuno` |
| Control Flow | `ruko` | Exit a `jabtak` loop or `chuno` block |
| Control Flow | `jaari` | Skip to the next `jabtak` iteration |
| Comparator | `barabar` | Equality comparison (`==`) |
| Comparator | `alag` | Inequality comparison (`!=`) |
| Comparator | `chhota` | Less-than comparison (`<`) |
| Comparator | `bada` | Greater-than comparison (`>`) |
| Comparator | `<=` | Less-than-or-equal comparison |
| Comparator | `>=` | Greater-than-or-equal comparison |
| Built-in | `likho` | Output text or integer values |
| Built-in | `niklo` | Structured program termination |

Table 1. BhasaCore keyword and comparator mapping.

## Example Program

```txt
namaste() {
  likho("BHASACORE DEMO SHURU");

  ginti total = 2 + 3 * 4;
  likho(total);

  agar(total barabar 14) {
    likho("hisab sahi hai");
  } warna {
    likho("hisab galat hai");
  }

  ginti i = 0;
  jabtak(i chhota 3) {
    likho(i);
    i = i + 1;
  }

  niklo(0);
}
```

## Compiler Stages

### 1. Lexical Analysis

`lexerf.c` tokenizes keywords, identifiers, integers, strings, separators, operators, and comparators.

It supports:

- line comments with `#` and `//`
- block comments with `/* ... */`
- a single Hindi keyword for each language construct

### 2. Parsing

`parserf.c` builds the abstract syntax tree and handles:

- program entry blocks
- variable declarations and assignments
- arithmetic expressions with precedence
- `agar` / `warna`
- `jabtak`
- `chuno` / `mamla` / `baki`
- `ruko` and `jaari`
- `likho(...)`
- `niklo(...)`

### 3. Semantic Analysis

`semanticf.c` validates the AST before code generation. It checks:

- variable use before declaration
- duplicate declarations in the same scope
- invalid `ruko` outside loop or switch
- invalid `jaari` outside loop
- duplicate `mamla` values inside a `chuno` block

### 4. Optimization

`optimizerf.c` rewrites the AST after semantic checks and before assembly generation. It currently performs:

- constant folding for arithmetic expressions
- constant-condition simplification for `agar`, `jabtak`, and `chuno`
- dead code elimination for unreachable statements after `niklo`, `ruko`, `jaari`, and fully-terminating branches
- CFG-style fallthrough analysis to detect whether later statements in a block are reachable

### 5. Code Generation

`codegeneratorf.c` emits x86-64 assembly.

- NASM syntax is used when NASM is available
- GAS Intel syntax is used automatically when NASM is missing and GCC fallback assembly is needed

### 6. Native Build Driver

`main.c` coordinates the full build:

- reads `.hin` source input
- runs lexer, parser, semantic analysis, optimization, and code generation
- assembles generated output
- links the final executable with GCC

## Supported Language Features

- integer declarations
- reassignment
- arithmetic with `+`, `-`, `*`, `/`, `%`
- parenthesized expressions
- string output
- integer output
- `agar` / `warna`
- `jabtak`
- `chuno`, `mamla`, `baki`
- `ruko` and `jaari`
- comments with `#`, `//`, and `/* ... */`

Optimization passes:

- constant folding
- dead code elimination
- CFG-style reachability analysis on statement lists

## Temporary File Behavior

For full native builds, BhasaCore generates temporary assembly and object files using the compiler process ID, for example:

```text
temp_12345.asm
temp_12345.o
```

Behavior:

- successful full builds remove temporary files automatically
- `--keep-temps` or `-k` keeps temporary files for inspection
- failed builds leave temporary files in place for debugging
- `--asm-only` writes the final assembly to `<output>.asm`

## Toolchain Requirements

### Linux / Ubuntu / WSL

```sh
sudo apt-get update
sudo apt-get install -y gcc nasm make
```

### Windows

Install `gcc` and ensure it is available in `PATH`. NASM is recommended, but native builds can still work without it because BhasaCore falls back to GCC's assembler when needed.

Common options:

- MSYS2 / MinGW-w64 for `gcc`
- NASM official installer for `nasm`

On Windows, BhasaCore also checks common NASM install locations such as `%LOCALAPPDATA%\bin\NASM\nasm.exe` if `nasm` is not already available in `PATH`.

Verify your setup with:

```powershell
gcc --version
nasm --version
```

## Building

### Linux / WSL with Makefile

```sh
make
```

Clean build output:

```sh
make clean
```

### Linux / WSL with shell script

```sh
bash build.sh
```

### Windows Command Prompt

```bat
build.cmd
```

### Windows PowerShell

```powershell
.\build.ps1
```

## Command-Line Usage

### Compile a `.hin` program

Windows:

```powershell
.\build\bhasacore.exe .\examples\hello.hin hello_demo
```

Linux / WSL:

```sh
./build/bhasacore ./examples/hello.hin hello_demo
```

### Generate assembly only

Windows:

```powershell
.\build\bhasacore.exe .\examples\full_demo.hin full_demo --asm-only
```

Linux / WSL:

```sh
./build/bhasacore ./examples/full_demo.hin full_demo --asm-only
```

### Keep temporary files

```powershell
.\build\bhasacore.exe .\examples\hello.hin hello_demo --keep-temps
```

### Show compiler stages

```powershell
.\build\bhasacore.exe .\examples\full_demo.hin full_demo --emit-tokens --dump-ast --dump-symbols --asm-only
```

### Show help

```powershell
.\build\bhasacore.exe --help
```

Supported flags:

- `--emit-tokens`
- `--dump-ast`
- `--dump-symbols`
- `--asm-only`
- `--keep-temps`
- `-k`
- `--help`
- `-h`
- `--no-editor`

## Windows Editor

Running the compiler with no command-line arguments opens the built-in editor.

```powershell
.\build\bhasacore.exe
```

Use `--no-editor` to print CLI usage instead of launching the GUI:

```powershell
.\build\bhasacore.exe --no-editor
```

Editor features:

- open and save `.hin` source files
- compile directly from the GUI
- run the generated executable
- show compiler and program output in the output pane

## Project Structure

```text
.
|-- main.c
|-- diagnostics.c / diagnostics.h
|-- lexerf.c / lexerf.h
|-- parserf.c / parserf.h
|-- semanticf.c / semanticf.h
|-- optimizerf.c / optimizerf.h
|-- codegeneratorf.c / codegeneratorf.h
|-- editor_win.c / editor_win.h
|-- examples/
|-- hashmap/
|-- Makefile
|-- build.cmd
|-- build.ps1
`-- build.sh
```

## Example Programs

| Program | Features Exercised |
| --- | --- |
| `examples/hello.hin` | `namaste`, `likho`, string literal |
| `examples/arithmetic.hin` | `ginti`, arithmetic operators, expressions |
| `examples/conditionals.hin` | `agar`, `warna`, relational operators |
| `examples/nested_loops.hin` | `jabtak`, nested loops, `ruko` |
| `examples/fibonacci.hin` | loops, variables, iterative computation |
| `examples/factorial.hin` | loops, arithmetic, control flow |
| `examples/switch_stmt.hin` | `chuno`, `mamla`, `baki`, `ruko`, `jaari` |
| `examples/output_test.hin` | `likho`, string and integer output |
| `examples/optimizations.hin` | constant folding, dead code elimination, constant control-flow pruning |
| `examples/break_continue.hin` | `ruko`, `jaari` inside loops |
| `examples/nested_if.hin` | nested `agar` conditions |
| `examples/error_undecl.hin` | undeclared variable detection |
| `examples/error_types.hin` | invalid integer-only usage checks |
| `examples/error_syntax.hin` | multiple syntax errors and recovery |
| `examples/full_pipeline.hin` | complete compilation (lexer -> GCC executable) |

Intentional failure cases:

- `error_undecl.hin`, `error_types.hin`, and `error_syntax.hin` are meant to be compiled to inspect diagnostics
- the remaining examples should compile into runnable executables

Additional bundled demos:

- `examples/loop.hin`
- `examples/control_flow.hin`
- `examples/fizzbuzz.hin`
- `examples/full_demo.hin`
- `examples/namaste_demo.hin`

## Why This Project Fits Compiler Design

BhasaCore demonstrates the full workflow expected in a compiler design project:

- source language design
- tokenization
- syntax analysis
- semantic analysis
- target code generation
- assembly and linking
- execution of the final native binary

## Current Scope

BhasaCore is intentionally compact and focused on clarity. It is suitable for classroom demonstration, project evaluation, and explaining how a complete compiler pipeline works from source code to executable output.
