# mibicc

A tiny C99 compiler written in C, targeting both x86-64 Linux (System V AMD64 ABI) and Windows (Microsoft x64 ABI).

[![CI](https://github.com/mi-vault/mibicc/actions/workflows/ci.yml/badge.svg)](https://github.com/mi-vault/mibicc/actions/workflows/ci.yml)
![Target](https://img.shields.io/badge/target-x86--64%20Linux%20%7C%20Windows-blue)
![Standard](https://img.shields.io/badge/C%20Standard-C99-orange)
![License](https://img.shields.io/badge/license-MIT-green)

`mibicc` is an educational, dependency-free C compiler designed to be clean, readable, and incrementally verifiable. Unlike many hobby compilers that are hardcoded to the Linux System V ABI, `mibicc` supports target selection for both Linux and Windows environments.

Detailed architectural decisions, edge cases, and learnings are documented in the [Lab Notebook](DEVLOG.md).

## Architecture

`mibicc` is constructed incrementally following modern compiler construction principles:

```
[ Source C Code ]
        │
        ▼
   [ Tokenizer ]
        │
        ▼
 [ Recursive Descent Parser ] -> Abstract Syntax Tree (AST)
        │
        ▼
  [ Stack Machine Code Generator ]
        │
        ├──> x86-64 Linux (System V ABI)
        └──> x86-64 Windows (Microsoft x64 ABI)
        │
        ▼
   [ as / gcc / clang ] -> Native Executable
```

## Features

- **Dependency-Free**: Pure C99 using only standard headers (`stdio.h`, `stdlib.h`, `string.h`, `ctype.h`).
- **AST-Based Parser**: Full recursive descent parser generating explicit `Node` trees with precedence levels.
- **Stack Machine Codegen**: Emits x86-64 assembly utilizing `push`, `pop`, `imul`, `cqo`, and `idiv`.
- **Visual Diagnostics**: Clang-style error diagnostics with source line printing and `^` caret pointing.
- **Cross-Platform Target Flags**: Supports `--target=linux` and `--target=windows`.

## Usage

```bash
# Compile expressions, function calls, local variables, and control flow to x86-64 assembly
./mibicc "sum = 0; for (i = 1; i <= 10; i = i + 1) sum = sum + i; return sum;" > out.s

# Function call passing arguments via System V AMD64 or Windows x64 ABI
./mibicc "return add(20, 22);" > out.s

# Specify a target ABI explicitly
./mibicc --target=linux "val = 10; if (val == 10) return add(20, 22); else return 0;" > out.s
./mibicc --target=windows "return win_add(20, 22);" > out.s

# Assemble and link with GCC or Clang
gcc -static -o out out.s
./out
echo $?  # Outputs the evaluated result
```

## Roadmap

- [x] **Step 1:** Integer constants and process exit codes (`mov $N, %rax; ret`)
- [x] **Step 2:** Basic arithmetic (`+`, `-`)
- [x] **Step 3:** Tokenizer with whitespace handling and visual caret diagnostics
- [x] **Step 4:** Operator precedence (`*`, `/`, nested parentheses) & cross-platform targets
- [x] **Step 5:** Unary operators (`+`, `-`)
- [x] **Step 6:** Equality and relational comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- [x] **Step 7:** Local variables and assignment
- [x] **Step 8:** Statements and semicolons (`return`, multi-letter identifiers)
- [x] **Step 9:** Control flow (`if`/`else`, `while`, `for`)
- [x] **Step 10:** Block statements `{ ... }`
- [x] **Step 11:** Function calls with System V and Windows x64 calling conventions
- [ ] **Step 12:** Pointer operations and dereferencing (`&`, `*`)
- [ ] **Step 13:** Pointer arithmetic and arrays
- [ ] **Step 14:** Structs, unions, and typedefs
- [ ] **Step 15:** Self-hosting

## Building and Testing

Prerequisites: A standard C compiler (`gcc` or `clang`) and `make`.

```bash
# Build mibicc
make

# Run the test suite
make test
```

## License

MIT
