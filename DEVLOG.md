# mibicc Devlog & Lab Notebook

A collection of notes, architectural decisions, "aha!" moments, and tricky edge cases encountered while incrementally building `mibicc` from scratch.

---

## 1. Dual-Target ABI Architecture (Linux System V vs. Microsoft x64)

Most educational and hobby compilers hardcode the System V AMD64 ABI (Linux/macOS) and leave Windows as an afterthought. Because development happens on a Windows machine, supporting Windows natively from the start (`--target=windows` and `--target=linux`) was a deliberate design choice.

### Key ABI Differences on x86-64:
- **Integer Argument Registers**:
  - System V AMD64: `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`
  - Microsoft x64: `%rcx`, `%rdx`, `%r8`, `%r9`
- **Shadow Space (Home Space)**:
  - Windows requires callers to allocate 32 bytes of "shadow space" on the stack immediately before issuing a `call` instruction, even if the callee takes fewer than 4 arguments.
- **Stack Alignment**:
  - Both ABIs require `%rsp` to be 16-byte aligned before the `call` instruction.

---

## 2. Edge Case: The Leading Minus CLI Flag Conflict

When implementing Step 5 (unary operators), running `./mibicc "-10 + 20"` produced a surprising error:
```
unknown option: -10 + 20
```

### The Root Cause:
The CLI argument parser looped while `argv[i][0] == '-'`. It assumed any argument beginning with a hyphen was a compiler option flag. Because negative expressions like `"-10 + 20"` also begin with a hyphen, the option parser intercepted the expression argument.

### The Solution:
Restructured argument parsing to strictly check for known option prefixes (`--target=`, `-h`, `--help`, and standard `--` terminator) rather than treating any leading `-` as a flag.

---

## 3. Infix vs. Prefix Negation

Unary `-` shares the same character as binary `-` but has distinct precedence and semantics:
- Binary `-` is left-associative with low precedence (`expr = mul ("+" mul | "-" mul)*`).
- Unary `-` is right-associative with higher precedence (`unary = ("+" | "-") unary | primary`).

### Language Comparisons:
- **SML**: Sidesteps token ambiguity entirely by using `~` for unary negation and `-` exclusively for subtraction.
- **Haskell**: Tokenizes `-` uniformly, then converts prefix occurrences to a `negate` function in the parser.
- **mibicc**: Recursive descent cleanly separates precedence levels. Unary `-` recursively calls `unary()` to generate an `ND_NEG` AST node, emitting `neg %rax` in the stack machine. This naturally supports chaining like `- - -5 + 10`.

---

## 4. Test Suite Archive & Community Insights

### The "39" Block Scope Test (Saved for Steps 7 & 10)
Suggested on r/Compilers by the community to verify variable shadowing and scope restoration in a single assertion:

```c
// Step 7/10 target assertion
int x = 3;
int y = 0;
{
    int x = 9;
    y = x;
}
return 10 * x + y; // Evaluates to 39
```
- If inner scope lookup succeeds: `y` receives `9`.
- If outer scope is properly preserved and restored: `x` evaluates to `3`.
- Final result: `10 * 3 + 9 = 39`.

### Two-Stack vs. Standard Call Stack
Discussed the tradeoffs of dual-stack machines (one stack for expression evaluation, one for call frames/locals) vs. single hardware stack. While dual stacks simplify temporary bookkeeping, adhering to the single hardware stack with `%rbp` offsets is necessary for standard C library interoperability (`printf`, `malloc`, libc).

---

## 5. Boolean Comparison Sequence & The Operand Swap Trick

Step 6 introduced comparison and relational operators (`==`, `!=`, `<`, `<=`, `>`, `>=`).

### Two-Step Comparison Pattern:
In x86-64 assembly, comparison instructions (`cmp`) update condition flags (ZF, SF, OF, CF) rather than placing a boolean 0 or 1 directly in a general-purpose register. To produce a C-standard integer 0 or 1:
```assembly
cmp %rdi, %rax     # Compare operands
setl %al           # Set %al byte to 1 if less, else 0
movzb %al, %rax    # Zero-extend byte in %al to full 64-bit %rax
```

### The Operand Swap Trick:
Instead of defining distinct AST nodes and code generation routines for greater-than (`>`) and greater-or-equal (`>=`), the parser swaps the left-hand and right-hand expressions:
- `a > b` is structurally identical to `b < a` (`ND_LT` with swapped children).
- `a >= b` is structurally identical to `b <= a` (`ND_LE` with swapped children).
This halves the required comparison node kinds and assembly instructions without altering semantics.

---

## 6. Lvalues, Variable Offsets, and Stack Frame Management

Step 7 added single-letter local variables (`a-z`), assignment (`=`), and multi-statement execution.

### Lvalue vs. Rvalue Resolution:
An lvalue represents a memory address that can be stored to, whereas an rvalue is a value computed by an expression:
- When compiling an identifier on the left of an assignment (`a = 3`), `gen_addr()` computes the variable's memory address using `lea -offset(%rbp), %rax`.
- When evaluating an identifier in an expression (`a + 2`), `gen()` calls `gen_addr()`, pops the address, and immediately dereferences it with `mov (%rax), %rax`.

### Stack Frame Alignment:
Every unique local variable allocates an 8-byte slot below the base pointer (`-8(%rbp)`, `-16(%rbp)`, etc.). To satisfy the 16-byte stack alignment mandated by both System V AMD64 and Windows x64 ABIs before potential function calls, the total local variable space is rounded up to the nearest multiple of 16:
```c
int stack_size = ((nlocals * 8) + 15) & ~15;
```

### Statement Discarding & Return Semantics:
In multi-statement programs (`stmt; stmt; ...`), every statement pushes its final result onto the stack. To prevent stack leakage across statements, all intermediate results are discarded with `pop %rax` except for the final statement, whose value remains in `%rax` as the program's exit code.

---

## 7. Architectural Models: Vertical Slicing vs. Horizontal Staging

Reflections prompted by technical discussions with fellow compiler authors in the community (notably Marc Jacob, author of `MarcJacob/C-Compiler`).

### The Two Methodologies:
- **Horizontal Staging**: Build the compiler phase-by-phase across the entire grammar before generating code (Full Tokenizer -> Full AST / Parser -> Semantic Analysis / Type Solver -> Code Generator).
  - *Advantage*: High architectural purity early on; data structures and module boundaries are designed with full foresight of standard requirements.
  - *Drawback*: Months of development before the first binary executes; codegen surprises (ABI quirks, register allocation bottlenecks) are deferred to the very end.
- **Vertical Slicing (mibicc approach)**: Build thin, end-to-end slices from source text directly to runnable machine code and execution verification, incrementally widening the supported grammar.
  - *Advantage*: Immediate feedback loop and psychological momentum; real-world ABI constraints (Windows 32-byte shadow space, System V vs MS x64 register passing) are confronted and solved at Step 1 rather than deferred.
  - *Drawback*: Refactoring tax. When fundamental abstractions evolve (e.g. adding static types or struct layouts), changes must ripple through every stage (lexer, AST, codegen) simultaneously.

### The C Declaration Grammar Revelation:
In standard C99, top-level grammar fundamentally reduces to object declarations:
```
[storage-class / typedef] [type-specifier] [declarator-list];
```
Because type definitions and object instantiations share the same declaration specifier grammar, C permits seemingly convoluted constructs where types are defined in-place during declarator evaluation:
```c
typedef enum { VAL_1, VAL_2 } array_of_anonymous_enum[25];

struct foo {
    int result;
    const char *str;
    array_of_anonymous_enum array;
} bar(int a, int b) {
    // Valid C function definition declaring return struct type simultaneously
    return (struct foo){ a + b, "valid C99", {0} };
}
```
Understanding that declarations always follow this pattern collapses what initially appears to be a fragmented parsing tree into a coherent state machine: identify the type specifier, then iterate through comma-separated declarators (pointers, identifiers, array bounds, parameter lists) until terminating at a semicolon or a function body `{`.

### The Modularity Threshold:
While keeping `mibicc` in a single translation unit (`src/main.c`) enabled rapid prototyping through Step 7, scaling past Step 10 (types, pointers, and structs) will necessitate modularizing into distinct files:
- `tokenize.c` (lexer and caret diagnostics)
- `parse.c` (recursive descent and AST construction)
- `type.c` (type checker and implicit promotions)
- `codegen.c` (ABI-specific assembly emission)

---

## 8. Return Statements, Multi-Letter Identifiers, and Epilogue Jump Optimization

Step 8 introduces explicit `return` statements (`ND_RETURN`), arbitrary-length local identifiers, and a unified function epilogue label.

### Statement Discarding vs. Explicit Return:
In C, an expression followed by a semicolon is an expression statement (`expr;`). Its evaluation produces a value that must be discarded to keep the hardware stack balanced across statements:
- For expression statements, codegen emits `pop %rax` immediately after evaluating the expression.
- For `return expr;`, codegen evaluates the expression into `%rax` and emits `jmp .L.return`.
- This ensures any subsequent statements are cleanly skipped when an early `return` executes.
- Because `%rax` is only modified when evaluating expressions, programs without explicit `return` (legacy Step 1-7 tests) naturally preserve the value of the last statement in `%rax` as the exit code.

### Generalizing Identifiers from Single Chars to Length-Bounded Strings:
Previous iterations stored local variables using a single `char name`. To support realistic code, identifiers now point to their source slice (`char *name; int len`):
- `is_ident1()` checks for `[a-zA-Z_]` to start an identifier.
- `is_ident2()` allows alphanumeric characters plus underscores for continuation.
- Keyword recognition for `"return"` verifies that the trailing character is not an identifier character (`!is_ident2(p[6])`), avoiding false positives for identifiers like `return_code` or `returnval`.
- Local variable lookup uses `memcmp` against existing slice lengths, allocating slots below `%rbp` (`-8`, `-16`, etc.) with 16-byte stack frame alignment maintained for downstream ABI compliance.



