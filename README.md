# Unnamed-lang

Unnamed-lang is a small educational compiler written in C. It takes a `.unn` source file, tokenizes it, builds a syntax tree, generates x86-64 NASM assembly, and links that assembly into a native executable. On Windows, the project also includes a lightweight editor window for writing, compiling, and running programs from a simple GUI.

```text
.unn source -> lexer -> parser -> code generator -> generated.asm -> executable
```

## Highlights

- End-to-end compiler pipeline implemented in plain C
- x86-64 assembly output through NASM
- Native executable generation through GCC
- Windows editor mode with open, save, compile, and output panes
- Example programs for loops, branching, switch statements, and printing

## Example Program

```txt
write("HELLO FROM UNNAMED-LANG", 24);

int number = 42;
write(number, 2);

exit(0);
```

## What The Compiler Does

1. `lexerf.c` reads source text and converts it into tokens.
2. `parserf.c` turns those tokens into the tree structure used by the compiler.
3. `codegeneratorf.c` walks that structure and emits NASM assembly.
4. `main.c` assembles and links the generated output into a runnable binary.

## Supported Language Features

- Integer variable declarations and reassignment
- Integer arithmetic with `+`, `-`, `*`, `/`, and `%`
- Comparisons with language keywords instead of C-style operators
- `if` / `else`
- `while`
- `switch`, `case`, and `default`
- `break` and `continue`
- `write(...)` for text and integer output
- `exit(...)`

## Comparison Keywords

| Keyword | Meaning |
| --- | --- |
| `eq` | `==` |
| `neq` | `!=` |
| `less` | `<` |
| `greater` | `>` |

Example:

```txt
while(i neq 10){
  i = i + 1;
}
```

## Current Limitations

- No user-defined functions yet
- No input support
- No floating-point support
- No arrays or string variables

## Control-Flow Validation

The compiler rejects several invalid control-flow patterns:

- `else` without a matching `if`
- `case` or `default` outside a `switch`
- More than one `default` inside the same `switch`
- `break;` outside `while` or `switch`
- `continue;` outside `while`

## Project Layout

```text
.
|-- main.c
|-- lexerf.c / lexerf.h
|-- parserf.c / parserf.h
|-- codegeneratorf.c / codegeneratorf.h
|-- editor_win.c / editor_win.h
|-- hashmap/
|-- examples/
|-- build.cmd
|-- build.ps1
`-- build.sh
```

## Requirements

- GCC
- NASM
- Windows editor mode additionally links against `user32`, `gdi32`, and `comdlg32`

## Build And Run

### Windows PowerShell

Build the compiler:

```powershell
.\build.ps1
```

If PowerShell script execution is disabled on your machine, use `.\build.cmd` instead.

Compile a sample program:

```powershell
.\build\unn.exe .\examples\print.unn print_demo
```

Run the generated program:

```powershell
.\print_demo.exe
```

Open the built-in editor window:

```powershell
.\build\unn.exe
```

The editor can open or save `.unn` files, compile them, run the generated executable automatically, and show compiler plus program output in the lower output pane.

### Windows Command Prompt

```bat
build.cmd
build\unn.exe examples\print.unn print_demo
print_demo.exe
```

### Linux Or WSL

Build the CLI compiler:

```sh
bash build.sh
```

Compile and run an example:

```sh
./build/unn ./examples/fizzbuzz.unn fizzbuzz_demo
./fizzbuzz_demo
```

The GUI editor is Windows-only. On non-Windows platforms the project builds and runs in command-line mode.

## Example Programs

| File | What it shows |
| --- | --- |
| `examples/print.unn` | Basic text and integer output |
| `examples/while.unn` | Counting loop with repeated output |
| `examples/fizzbuzz.unn` | Arithmetic, conditionals, and nested checks |
| `examples/control_flow.unn` | `while`, `switch`, `break`, and `continue` |
| `examples/full_demo.unn` | A broader language walkthrough |

## Notes For GitHub

- Build outputs and generated binaries are ignored through `.gitignore`
- A basic GitHub Actions workflow is included to smoke-test the CLI compiler on pushes and pull requests
- A license is not included yet, so choose one deliberately before publishing if you want the repository to be open-source
