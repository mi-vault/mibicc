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
