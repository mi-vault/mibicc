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

---

## 9. Control Flow Statements and Compound Blocks

Step 9 expands the statement grammar beyond linear sequences into structured control flow: conditional branching (`if`/`else`), bounded and unbounded iteration (`while`, `for`), compound statement blocks (`{ ... }`), and empty statements (`;`).

### Separating Expressions and Statements in Codegen:
A critical architectural milestone reached in Step 9 is the formal decoupling of expression evaluation (`gen_expr`) from statement execution (`gen_stmt`):
- `gen_expr(Node *node)` computes a value and leaves exactly one 64-bit integer on the hardware stack.
- `gen_stmt(Node *node)` executes for side effects and flow control, guaranteeing that the stack depth is completely neutral (net change of zero) across every statement.
- An expression statement (`ND_EXPR_STMT`) simply evaluates `node->lhs` with `gen_expr` and immediately executes `pop %rax`.
- This clean separation eliminates stack leakage inside loops and complex nested branches without requiring ad-hoc stack depth tracking.

### Dangling Else and Recursive Descent:
In standard C, an `else` binds to the innermost open `if`. In a recursive descent parser, greedy consumption handles this naturally without extra lookahead:
```c
if (consume("if")) {
    ...
    node->then = stmt();
    if (consume("else"))
        node->els = stmt();
    return node;
}
```
Because the inner `stmt()` call parses the nested `if` and eagerly tests for `else` before returning, `else` automatically associates with the closest preceding `if`.

### Loop Labels and Sequence Monotonicity:
Loops (`while`, `for`) and conditionals (`if`) require local branch targets for jump instructions (`je`, `jmp`). Using a monotonic integer counter (`label_seq++`) ensures every generated label (`.L.begin.N`, `.L.else.N`, `.L.end.N`) is globally unique across the translation unit. This cleanly supports arbitrarily deep loop nesting and multi-level branching.

---

## 10. Function Calls and Cross-Platform Calling Conventions (System V AMD64 vs. Microsoft x64)

Step 11 introduces function calls (`ND_FUNCALL`), parsing comma-separated argument lists, and handling x86-64 calling conventions and stack alignment rules for both Linux and Windows target ABIs.

### Parsing Function Calls Without Variable Pollution:
In the lexer and parser, an identifier can represent either a variable reference (`x`) or a function call (`foo(...)`):
- When `token->kind == TK_IDENT`, the parser inspects the immediately following token.
- If followed by `(`, it is parsed as a function call without invoking `find_or_create_var()`, preventing function names from polluting the local variable table or allocating stack frame slots.
- Arguments are parsed as a comma-separated list of general expressions (`expr()`), allowing arbitrary nesting (e.g. `add(add(10, 20), mul(3, 4))`), variable arguments, and control flow integration.

### System V AMD64 Calling Convention (Linux, macOS, BSD):
Under the System V AMD64 ABI:
1. **Argument Registers**: The first 6 integer or pointer arguments are passed in registers in exact order:
   `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.
2. **Evaluation & Pop Order**: Arguments are evaluated left-to-right using `gen_expr()`, pushing their 64-bit results onto the stack. They are then popped in reverse order into the designated registers.
3. **16-Byte Stack Alignment**: The AMD64 ABI requires `(%rsp + 8)` to be a multiple of 16 at function entry, which means `%rsp` must be 16-byte aligned (`%rsp % 16 == 0`) immediately prior to the `call` instruction. Because function calls can occur inside complex expressions where intermediate values reside on the stack, `mibicc` emits a dynamic alignment check:
   ```assembly
     test $15, %rsp
     jnz .L.call.align.N
     mov $0, %al
     call <funcname>
     jmp .L.call.end.N
   .L.call.align.N:
     sub $8, %rsp
     mov $0, %al
     call <funcname>
     add $8, %rsp
   .L.call.end.N:
     push %rax
   ```
4. **Variadic Register Guard (`%al = 0`)**: For variadic functions like `printf`, `%al` must denote the number of vector registers used (0 here) to prevent crashes in the callee.

### Microsoft x64 Calling Convention (Windows):
Under the Microsoft x64 ABI:
1. **Argument Registers**: The first 4 integer or pointer arguments are passed in:
   `%rcx`, `%rdx`, `%r8`, `%r9`.
2. **Shadow Space (Home Space)**: The caller is required to reserve at least 32 bytes (4 quadwords) of stack space directly above the return address, even if the function takes zero arguments.
3. **Stack Arguments (5th and 6th Arguments)**: If more than 4 arguments are passed, arguments 5 and 6 are placed on the stack at `32(%rsp)` and `40(%rsp)` above the shadow space. Frame allocation uses 48 bytes (`sub $48, %rsp`) to ensure strict 16-byte stack alignment.
4. **Caller Cleanup**: After the call returns, the caller cleans up the allocated shadow/argument area (`add $32, %rsp` or `add $48, %rsp`), and pushes the return value in `%rax` back onto the stack machine.

### Native Cross-ABI Verification:
To verify Windows ABI code generation on Linux in CI and local testing, helper functions in `test_helpers.c` are decorated with `__attribute__((ms_abi))`. This causes GCC/Clang on Linux to compile test routines with the exact Microsoft x64 register allocation and shadow space requirements, validating cross-platform compatibility without needing a Windows host.

---

## 11. Pointer Operations (`&`, `*`) and Windows Stack-Arg Alignment Edge Case

Step 12 adds unary address-of (`&`) and pointer dereference (`*`) operators (`ND_ADDR` and `ND_DEREF`), along with a subtle Windows x64 ABI alignment fix uncovered during regression testing.

### Untyped Pointers as 64-Bit Addresses:
Because `mibicc` has not yet introduced static type declarations (`int`, pointer types, `sizeof`), every local variable and pointer occupies a uniform 8-byte slot on the stack:
- **`&` in Lexer & Parser**: `&` is added to the single-character punctuator set, and `unary()` handles both `&` (`ND_ADDR`) and `*` (`ND_DEREF`) right-associatively (`unary = ("+" | "-" | "&" | "*") unary | primary`).
- **Precedence**: Unary `*` binds tighter than binary `*`, so `*p * *q` cleanly parses as `(*p) * (*q)`, and `-*p` parses as `-(*p)`.

### Generalizing `gen_addr()` for Dereference Lvalues:
Previously, `gen_addr()` only accepted `ND_VAR` and rejected every other node with `"not an lvalue"`. To support writing through pointers (`*p = 5`, `**q = 42`, `*identity_long(&x) = 77`):
- **`ND_VAR` in `gen_addr()`**: Emits `lea offset(%rbp), %rax` and pushes the variable's stack address.
- **`ND_DEREF` in `gen_addr()`**: Evaluates `node->lhs` via `gen_expr()`. Because a pointer expression already evaluates to a memory address on the stack, its rvalue *is* the target lvalue address.
- **`ND_ADDR` in `gen_expr()`**: Simply calls `gen_addr(node->lhs)` and leaves the address on the stack; invalid targets like `&3`, `&(x + 1)`, or `&&x` still trigger `"not an lvalue"`.
- **`ND_DEREF` in `gen_expr()`**: Evaluates `node->lhs` to get the address, pops it into `%rax`, loads the 64-bit value with `mov (%rax), %rax`, and pushes it back onto the stack.

Because `ND_ASSIGN` delegates LHS resolution to `gen_addr(node->lhs)`, assignments through arbitrarily nested pointers (`**q = 42`) and function return values (`*identity_long(&x) = 77`) work without special cases.

### Edge Case Fix: Misaligned Stack on Windows 5/6-Argument Calls:
Regression testing `1 + win_sum6(1, 2, 3, 4, 5, 6)` exposed a bug in Step 11's Windows x64 `ND_FUNCALL` emission:
- Evaluating `1` before the call pushes one 8-byte value, leaving `%rsp` at `8 (mod 16)` when `win_sum6` prepares its call frame.
- Previously, `mibicc` subtracted `shadow_bytes` (`48`), stored arguments 5 and 6 at `32(%rsp)` and `40(%rsp)`, and *then* ran `test $15, %rsp` / `sub $8, %rsp` to align the stack.
- That post-hoc `sub $8, %rsp` shifted the previously written 5th and 6th arguments to `40(%rsp)` and `48(%rsp)` right before `call`, causing the callee to read garbage from `32(%rsp)`!
- **Fix**: Check `test $15, %rsp` *before* allocating the shadow space + stack argument area (`emit_win_call`), subtracting either `shadow_bytes` (aligned) or `shadow_bytes + 8` (misaligned) so `32(%rsp)` and `40(%rsp)` always match the exact ABI offsets immediately before `call`.




