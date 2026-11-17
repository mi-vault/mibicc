# mibicc

A tiny C99 compiler written in C, targeting x86-64 Linux.

[![CI](https://github.com/mi-vault/mibicc/actions/workflows/ci.yml/badge.svg)](https://github.com/mi-vault/mibicc/actions/workflows/ci.yml)

## Architecture

`mibicc` is constructed incrementally following modern compiler construction principles:

```
[ Source C Code ]
        │
        ▼
   [ Tokenizer ]
        │
        ▼
 [ Recursive Descent Parser ] -> AST
        │
        ▼
  [ Code Generator ] -> x86-64 Assembly (AT&T syntax)
        │
        ▼
   [ as / gcc ] -> Native Binary
```

## Roadmap

- [x] **Step 1:** Integer constants and process exit codes (`mov $N, %rax; ret`)
- [ ] **Step 2:** Basic arithmetic (`+`, `-`)
- [ ] **Step 3:** Tokenizer and whitespace handling
- [ ] **Step 4:** Operator precedence (`*`, `/`, parentheses)
- [ ] **Step 5:** Unary operators (`+`, `-`)
- [ ] **Step 6:** Equality and relational comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- [ ] **Step 7:** Local variables and assignment
- [ ] **Step 8:** Statements and semicolons (`return`)
- [ ] **Step 9:** Control flow (`if`/`else`, `while`, `for`)
- [ ] **Step 10:** Block statements `{ ... }`
- [ ] **Step 11:** Function calls and parameter passing
- [ ] **Step 12:** Pointer operations and dereferencing (`&`, `*`)
- [ ] **Step 13:** Pointer arithmetic and arrays
- [ ] **Step 14:** Structs, unions, and typedefs
- [ ] **Step 15:** Self-hosting

## Building and Testing

Prerequisites: A standard C compiler (`gcc` or `clang`) and `make` on x86-64 Linux.

```bash
# Build mibicc
make

# Run the test suite
make test
```

## License

MIT
