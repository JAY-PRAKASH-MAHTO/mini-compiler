# Namaste DSL Compiler

Namaste DSL Compiler is an end-to-end compiler written in C for a Hindi-flavored domain-specific language. It reads `.dsl` source code, performs lexical analysis, parsing, semantic validation, assembly generation, and finally produces a native executable with NASM and GCC.

```text
source.dsl
  -> lexer
  -> parser / AST
  -> semantic analyzer
  -> x86-64 assembly generator
  -> NASM object file
  -> native executable
```

## What This Project Does

- Designs a custom DSL with Hindi-style keywords
- Compiles source programs all the way to native machine executables
- Exposes each compiler stage for debugging and demonstration
- Includes a Windows editor for writing, compiling, and running DSL programs

## Language Style

The language uses simple transliterated Hindi keywords so the syntax feels distinct from C-like teaching languages.

### Core words

| Category | DSL words |
| --- | --- |
| Program entry | `namaste()` |
| Integer type | `ginti`, `count` |
| Output | `likho(...)`, `dikhao(...)`, `report(...)` |
| Conditionals | `agar`, `warna`, `varna`, `when`, `otherwise` |
| Loops | `jabtak`, `repeat` |
| Switch flow | `chuno`, `mamla`, `baki`, `choose`, `option`, `fallback` |
| Loop/switch control | `ruko`, `stop`, `jaari`, `skip` |
| Program exit | `niklo(...)`, `finish(...)` |

### Comparators

| Meaning | Supported words |
| --- | --- |
| Equal | `barabar`, `same`, `eq`, `==` |
| Not equal | `alag`, `different`, `neq`, `!=` |
| Less than | `chhota`, `below`, `less`, `<` |
| Greater than | `bada`, `above`, `greater`, `>` |
| Less than or equal | `<=` |
| Greater than or equal | `>=` |

## Example Program

```txt
namaste() {
  likho("NAMASTE DSL SHURU");

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

## Compiler Pipeline

### 1. Lexical Analysis

`lexerf.c` scans the source file and converts text into tokens such as keywords, identifiers, numbers, separators, operators, strings, and comparators.

### 2. Parsing

`parserf.c` builds an abstract syntax tree. The parser supports:

- `namaste()` entry blocks
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

- use of variables before declaration
- duplicate declarations in the same scope
- invalid `ruko` / `stop` outside loop or switch
- invalid `jaari` / `skip` outside loop
- duplicate `mamla` / `case` values inside one switch

### 4. Code Generation

`codegeneratorf.c` walks the AST and emits x86-64 NASM assembly into `generated.asm`.

### 5. Native Build

`main.c` invokes:

- NASM to assemble `generated.asm` into `generated.o`
- GCC to link the object file into a runnable executable

## Supported Features

- integer declarations
- reassignment
- arithmetic with `+`, `-`, `*`, `/`, `%`
- parenthesized expressions
- `agar` / `warna`
- `jabtak`
- `chuno`, `mamla`, `baki`
- `ruko` and `jaari`
- string output
- integer output
- line comments with `#` and `//`

## Command-Line Usage

### Build the compiler

Windows:

```bat
build.cmd
```

PowerShell:

```powershell
.\build.cmd
```

Linux / WSL:

```sh
bash build.sh
```

### Compile a DSL program

Windows:

```powershell
.\build\unn.exe .\examples\hello.dsl hello_demo
```

Linux / WSL:

```sh
./build/unn ./examples/hello.dsl hello_demo
```

### Show compiler stages

```powershell
.\build\unn.exe .\examples\full_demo.dsl full_demo --emit-tokens --dump-ast --dump-symbols --asm-only
```

Available flags:

- `--emit-tokens`
- `--dump-ast`
- `--dump-symbols`
- `--asm-only`

### Run the generated program

Windows:

```powershell
.\hello_demo.exe
```

Linux / WSL:

```sh
./hello_demo
```

## Windows Editor

Running the compiler with no command-line arguments opens the built-in Windows editor:

```powershell
.\build\unn.exe
```

Editor features:

- open and save source files
- compile from the GUI
- run generated executables
- show compiler and program output in the output pane

## Project Structure

```text
.
|-- main.c
|-- lexerf.c / lexerf.h
|-- parserf.c / parserf.h
|-- semanticf.c / semanticf.h
|-- codegeneratorf.c / codegeneratorf.h
|-- editor_win.c / editor_win.h
|-- examples/
|-- hashmap/
|-- build.cmd
|-- build.ps1
`-- build.sh
```

## Example Programs

- `examples/hello.dsl` shows a minimal DSL program with output and arithmetic.
- `examples/loop.dsl` demonstrates `jabtak` loops.
- `examples/control_flow.dsl` demonstrates `agar`, `chuno`, `mamla`, `baki`, `ruko`, and `jaari`.
- `examples/fizzbuzz.dsl` demonstrates arithmetic, nested conditions, and output.
- `examples/full_demo.dsl` shows a larger end-to-end language walkthrough.
- `examples/namaste_demo.dsl` remains a compact Hindi-flavored showcase program.

## Requirements

- GCC
- NASM
- On Windows editor builds: `user32`, `gdi32`, and `comdlg32`

## Why This Fits an End-to-End Compiler Project

This repository demonstrates the full compiler workflow expected in a compiler design project:

- custom source language design
- tokenization
- syntax analysis
- semantic analysis
- target-code generation
- assembly and linking
- execution of the final native binary

## Current Scope

The current compiler is focused on a compact DSL and intentionally keeps the implementation small and understandable. It currently targets integer-based programs with control flow and output, which makes it suitable for classroom demonstration, project evaluation, and compiler pipeline explanation.
