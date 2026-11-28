#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Target operating system ABI
typedef enum {
    TARGET_LINUX,   // System V AMD64 ABI (Linux, BSD, macOS)
    TARGET_WINDOWS, // Microsoft x64 ABI (Windows)
} TargetOS;

#ifdef _WIN32
static TargetOS target_os = TARGET_WINDOWS;
#else
static TargetOS target_os = TARGET_LINUX;
#endif

// Token kind
typedef enum {
    TK_RESERVED, // Keywords or punctuators (+, -, *, /, (, ), etc.)
    TK_IDENT,    // Identifiers (single lowercase letter a-z)
    TK_NUM,      // Integer literals
    TK_EOF,      // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;

struct Token {
    TokenKind kind; // Token kind
    Token *next;    // Next token
    long val;       // If kind is TK_NUM, its numerical value
    char *loc;      // Token location in source
    int len;        // Token length
};

// AST Node kind
typedef enum {
    ND_ADD,       // +
    ND_SUB,       // -
    ND_MUL,       // *
    ND_DIV,       // /
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_NEG,       // Unary -
    ND_ASSIGN,    // =
    ND_VAR,       // Variable reference
    ND_NUM,       // Integer
    ND_RETURN,    // return statement
    ND_IF,        // if statement
    ND_WHILE,     // while statement
    ND_FOR,       // for statement
    ND_BLOCK,     // { ... } block
    ND_EXPR_STMT, // Expression statement
    ND_FUNCALL,   // Function call
    ND_ADDR,      // Unary & (address-of)
    ND_DEREF,     // Unary * (dereference)
} NodeKind;

// Local variable
typedef struct Var Var;

struct Var {
    char *name;   // Pointer to variable name in source
    int len;      // Variable name length
    int offset;   // Offset from %rbp (negative, e.g. -8, -16, ...)
    Var *next;    // Next variable in the linked list
};

// AST Node type
typedef struct Node Node;

struct Node {
    NodeKind kind; // Node kind
    Node *lhs;     // Left-hand side
    Node *rhs;     // Right-hand side
    long val;      // Used if kind == ND_NUM
    Var *var;      // Used if kind == ND_VAR

    // "if", "while", or "for"
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    // Block "{ ... }"
    Node *body;
    Node *next;

    // Function call "func(arg1, arg2, ...)"
    char *funcname;
    Node *args[6];
    int nargs;
};

// Input program string
static char *user_input;

// Current token
static Token *token;

// All local variables created during parsing
static Var *locals;

// Reports an error and exits
static void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// Reports an error location with a visual caret pointer and exits
static void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, ""); // print pos spaces
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// Returns true if string p starts with prefix q
static bool startswith(char *p, char *q) {
    return strncmp(p, q, strlen(q)) == 0;
}

// Duplicates a string slice with explicit length into a null-terminated string
static char *strndup_len(const char *s, int len) {
    char *p = malloc(len + 1);
    if (!p)
        error("out of memory");
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

// Consumes the current token if it matches op
static bool consume(char *op) {
    if (token->kind != TK_RESERVED ||
        (int)strlen(op) != token->len ||
        memcmp(token->loc, op, token->len))
        return false;
    token = token->next;
    return true;
}

// Ensures that the current token is op
static void expect(char *op) {
    if (token->kind != TK_RESERVED ||
        (int)strlen(op) != token->len ||
        memcmp(token->loc, op, token->len))
        error_at(token->loc, "expected '%s'", op);
    token = token->next;
}

// Ensures that the current token is TK_NUM and returns its value
static long expect_number(void) {
    if (token->kind != TK_NUM)
        error_at(token->loc, "expected a number");
    long val = token->val;
    token = token->next;
    return val;
}

// Checks if we reached end-of-file
static bool at_eof(void) {
    return token->kind == TK_EOF;
}

// Creates a new token and links it to cur
static Token *new_token(TokenKind kind, Token *cur, char *loc, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc = loc;
    tok->len = len;
    cur->next = tok;
    return tok;
}

// Returns true if c can start an identifier
static bool is_ident1(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

// Returns true if c can be part of an identifier
static bool is_ident2(char c) {
    return is_ident1(c) || ('0' <= c && c <= '9');
}

// Tokenizes user_input and returns the head of the token list
static Token *tokenize(char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        // Skip whitespace
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Multi-character punctuators (==, !=, <=, >=)
        if (startswith(p, "==") || startswith(p, "!=") ||
            startswith(p, "<=") || startswith(p, ">=")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        // Keywords
        if (startswith(p, "return") && !is_ident2(p[6])) {
            cur = new_token(TK_RESERVED, cur, p, 6);
            p += 6;
            continue;
        }
        if (startswith(p, "if") && !is_ident2(p[2])) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "else") && !is_ident2(p[4])) {
            cur = new_token(TK_RESERVED, cur, p, 4);
            p += 4;
            continue;
        }
        if (startswith(p, "while") && !is_ident2(p[5])) {
            cur = new_token(TK_RESERVED, cur, p, 5);
            p += 5;
            continue;
        }
        if (startswith(p, "for") && !is_ident2(p[3])) {
            cur = new_token(TK_RESERVED, cur, p, 3);
            p += 3;
            continue;
        }

        // Single-character punctuators (+, -, *, /, (, ), <, >, =, ;, {, }, ,, &)
        if (strchr("+-*/()<>=;{},&", *p)) {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        // Integer literal
        if (isdigit(*p)) {
            char *q = p;
            long val = strtol(p, &p, 10);
            cur = new_token(TK_NUM, cur, q, p - q);
            cur->val = val;
            continue;
        }

        // Identifier
        if (is_ident1(*p)) {
            char *q = p++;
            while (is_ident2(*p))
                p++;
            cur = new_token(TK_IDENT, cur, q, p - q);
            continue;
        }

        error_at(p, "invalid token");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}

// Finds or creates a local variable by name and length
static Var *find_or_create_var(char *name, int len) {
    // Search existing variables
    for (Var *v = locals; v; v = v->next) {
        if (v->len == len && memcmp(v->name, name, len) == 0)
            return v;
    }

    // Allocate a new variable with the next stack slot
    Var *v = calloc(1, sizeof(Var));
    v->name = name;
    v->len = len;
    // Each variable occupies 8 bytes, offsets go -8, -16, -24, ...
    int count = 0;
    for (Var *x = locals; x; x = x->next)
        count++;
    v->offset = (count + 1) * -8;
    v->next = locals;
    locals = v;
    return v;
}

// Creates a new binary operator node
static Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

// Creates a new integer literal node
static Node *new_node_num(long val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

// Creates a new variable node
static Node *new_node_var(Var *var) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_VAR;
    node->var = var;
    return node;
}

// Parser forward declarations
static Node *expr(void);
static Node *assign(void);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *mul(void);
static Node *unary(void);
static Node *primary(void);

// primary = "(" expr ")" | ident "(" (expr ("," expr)*)? ")" | ident | num
static Node *primary(void) {
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    if (token->kind == TK_IDENT) {
        // Function call: ident "(" (expr ("," expr)*)? ")"
        if (token->next->kind == TK_RESERVED &&
            token->next->len == 1 &&
            *token->next->loc == '(') {
            char *name = strndup_len(token->loc, token->len);
            token = token->next->next; // skip ident and '('

            Node *node = calloc(1, sizeof(Node));
            node->kind = ND_FUNCALL;
            node->funcname = name;

            int nargs = 0;
            while (!consume(")")) {
                if (nargs > 0)
                    expect(",");
                if (nargs >= 6)
                    error_at(token->loc, "too many arguments to function call (max 6)");
                node->args[nargs++] = expr();
            }
            node->nargs = nargs;
            return node;
        }

        Var *var = find_or_create_var(token->loc, token->len);
        token = token->next;
        return new_node_var(var);
    }

    return new_node_num(expect_number());
}

// unary = ("+" | "-" | "&" | "*") unary | primary
static Node *unary(void) {
    if (consume("+"))
        return unary();
    if (consume("-"))
        return new_node(ND_NEG, unary(), NULL);
    if (consume("&"))
        return new_node(ND_ADDR, unary(), NULL);
    if (consume("*"))
        return new_node(ND_DEREF, unary(), NULL);
    return primary();
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul(void) {
    Node *node = unary();

    for (;;) {
        if (consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

// add = mul ("+" mul | "-" mul)*
static Node *add(void) {
    Node *node = mul();

    for (;;) {
        if (consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if (consume("-"))
            node = new_node(ND_SUB, node, mul());
        else
            return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(void) {
    Node *node = add();

    for (;;) {
        if (consume("<="))
            node = new_node(ND_LE, node, add());
        else if (consume("<"))
            node = new_node(ND_LT, node, add());
        else if (consume(">=")) {
            Node *rhs = add();
            node = new_node(ND_LE, rhs, node);
        } else if (consume(">")) {
            Node *rhs = add();
            node = new_node(ND_LT, rhs, node);
        } else
            return node;
    }
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality(void) {
    Node *node = relational();

    for (;;) {
        if (consume("=="))
            node = new_node(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_node(ND_NE, node, relational());
        else
            return node;
    }
}

// assign = equality ("=" assign)?
static Node *assign(void) {
    Node *node = equality();
    if (consume("="))
        node = new_node(ND_ASSIGN, node, assign());
    return node;
}

// expr = assign
static Node *expr(void) {
    return assign();
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | "{" stmt* "}"
//      | ";"
//      | expr ";"
static Node *stmt(void) {
    if (consume("return")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_RETURN;
        node->lhs = expr();
        expect(";");
        return node;
    }

    if (consume("if")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_IF;
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else"))
            node->els = stmt();
        return node;
    }

    if (consume("while")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_WHILE;
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if (consume("for")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_FOR;
        expect("(");
        if (!consume(";")) {
            node->init = expr();
            expect(";");
        }
        if (!consume(";")) {
            node->cond = expr();
            expect(";");
        }
        if (!consume(")")) {
            node->inc = expr();
            expect(")");
        }
        node->then = stmt();
        return node;
    }

    if (consume("{")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_BLOCK;
        Node head;
        head.next = NULL;
        Node *cur = &head;
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        node->body = head.next;
        return node;
    }

    if (consume(";")) {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_BLOCK;
        return node;
    }

    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_EXPR_STMT;
    node->lhs = expr();
    expect(";");
    return node;
}

static void gen_expr(Node *node);
static void gen_stmt(Node *node);

// Code generator: pushes the address of an lvalue onto the stack
static void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR:
        printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
        printf("  push %%rax\n");
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    default:
        error("not an lvalue");
    }
}

static int label_seq = 0;

// Argument-passing registers for x86-64 calling conventions
static char *argreg64_linux[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static char *argreg64_win[] = {"%rcx", "%rdx", "%r8", "%r9"};

// Emits a Windows x64 ABI call with shadow space + optional 5th/6th stack args
static void emit_win_call(Node *node, int alloc_bytes) {
    printf("  sub $%d, %%rsp\n", alloc_bytes);
    if (node->nargs >= 5)
        printf("  mov %%r10, 32(%%rsp)\n");
    if (node->nargs == 6)
        printf("  mov %%r11, 40(%%rsp)\n");
    printf("  call %s\n", node->funcname);
    printf("  add $%d, %%rsp\n", alloc_bytes);
}

// Code generator: evaluates expression using a stack machine
static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  push $%ld\n", node->val);
        return;

    case ND_VAR:
        gen_addr(node);
        printf("  pop %%rax\n");
        printf("  mov (%%rax), %%rax\n");
        printf("  push %%rax\n");
        return;

    case ND_ASSIGN:
        gen_addr(node->lhs);
        gen_expr(node->rhs);
        printf("  pop %%rdi\n");
        printf("  pop %%rax\n");
        printf("  mov %%rdi, (%%rax)\n");
        printf("  push %%rdi\n");
        return;

    case ND_NEG:
        gen_expr(node->lhs);
        printf("  pop %%rax\n");
        printf("  neg %%rax\n");
        printf("  push %%rax\n");
        return;

    case ND_ADDR:
        gen_addr(node->lhs);
        return;

    case ND_DEREF:
        gen_expr(node->lhs);
        printf("  pop %%rax\n");
        printf("  mov (%%rax), %%rax\n");
        printf("  push %%rax\n");
        return;

    case ND_FUNCALL: {
        for (int i = 0; i < node->nargs; i++)
            gen_expr(node->args[i]);

        int seq = label_seq++;

        if (target_os == TARGET_LINUX) {
            for (int i = node->nargs - 1; i >= 0; i--)
                printf("  pop %s\n", argreg64_linux[i]);

            printf("  test $15, %%rsp\n");
            printf("  jnz .L.call.align.%d\n", seq);
            printf("  mov $0, %%al\n");
            printf("  call %s\n", node->funcname);
            printf("  jmp .L.call.end.%d\n", seq);
            printf(".L.call.align.%d:\n", seq);
            printf("  sub $8, %%rsp\n");
            printf("  mov $0, %%al\n");
            printf("  call %s\n", node->funcname);
            printf("  add $8, %%rsp\n");
            printf(".L.call.end.%d:\n", seq);
            printf("  push %%rax\n");
            return;
        }

        if (target_os == TARGET_WINDOWS) {
            if (node->nargs == 6) {
                printf("  pop %%r11\n");
                printf("  pop %%r10\n");
            } else if (node->nargs == 5) {
                printf("  pop %%r10\n");
            }

            int reg_args = node->nargs < 4 ? node->nargs : 4;
            for (int i = reg_args - 1; i >= 0; i--)
                printf("  pop %s\n", argreg64_win[i]);

            int shadow_bytes = (node->nargs <= 4) ? 32 : 48;

            printf("  test $15, %%rsp\n");
            printf("  jnz .L.call.win.align.%d\n", seq);
            emit_win_call(node, shadow_bytes);
            printf("  jmp .L.call.win.end.%d\n", seq);
            printf(".L.call.win.align.%d:\n", seq);
            emit_win_call(node, shadow_bytes + 8);
            printf(".L.call.win.end.%d:\n", seq);
            printf("  push %%rax\n");
            return;
        }

        error("unsupported target for function call");
        return;
    }

    default:
        break;
    }

    gen_expr(node->lhs);
    gen_expr(node->rhs);

    printf("  pop %%rdi\n");
    printf("  pop %%rax\n");

    switch (node->kind) {
    case ND_ADD:
        printf("  add %%rdi, %%rax\n");
        break;
    case ND_SUB:
        printf("  sub %%rdi, %%rax\n");
        break;
    case ND_MUL:
        printf("  imul %%rdi, %%rax\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv %%rdi\n");
        break;
    case ND_EQ:
        printf("  cmp %%rdi, %%rax\n");
        printf("  sete %%al\n");
        printf("  movzb %%al, %%rax\n");
        break;
    case ND_NE:
        printf("  cmp %%rdi, %%rax\n");
        printf("  setne %%al\n");
        printf("  movzb %%al, %%rax\n");
        break;
    case ND_LT:
        printf("  cmp %%rdi, %%rax\n");
        printf("  setl %%al\n");
        printf("  movzb %%al, %%rax\n");
        break;
    case ND_LE:
        printf("  cmp %%rdi, %%rax\n");
        printf("  setle %%al\n");
        printf("  movzb %%al, %%rax\n");
        break;
    default:
        error("unknown expression kind");
    }

    printf("  push %%rax\n");
}

// Code generator: evaluates a statement
static void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        printf("  pop %%rax\n");
        return;

    case ND_RETURN:
        gen_expr(node->lhs);
        printf("  pop %%rax\n");
        printf("  jmp .L.return\n");
        return;

    case ND_BLOCK:
        for (Node *s = node->body; s; s = s->next)
            gen_stmt(s);
        return;

    case ND_IF: {
        int seq = label_seq++;
        if (node->els) {
            gen_expr(node->cond);
            printf("  pop %%rax\n");
            printf("  cmp $0, %%rax\n");
            printf("  je .L.else.%d\n", seq);
            gen_stmt(node->then);
            printf("  jmp .L.end.%d\n", seq);
            printf(".L.else.%d:\n", seq);
            gen_stmt(node->els);
            printf(".L.end.%d:\n", seq);
        } else {
            gen_expr(node->cond);
            printf("  pop %%rax\n");
            printf("  cmp $0, %%rax\n");
            printf("  je .L.end.%d\n", seq);
            gen_stmt(node->then);
            printf(".L.end.%d:\n", seq);
        }
        return;
    }

    case ND_WHILE: {
        int seq = label_seq++;
        printf(".L.begin.%d:\n", seq);
        gen_expr(node->cond);
        printf("  pop %%rax\n");
        printf("  cmp $0, %%rax\n");
        printf("  je .L.end.%d\n", seq);
        gen_stmt(node->then);
        printf("  jmp .L.begin.%d\n", seq);
        printf(".L.end.%d:\n", seq);
        return;
    }

    case ND_FOR: {
        int seq = label_seq++;
        if (node->init) {
            gen_expr(node->init);
            printf("  pop %%rax\n");
        }
        printf(".L.begin.%d:\n", seq);
        if (node->cond) {
            gen_expr(node->cond);
            printf("  pop %%rax\n");
            printf("  cmp $0, %%rax\n");
            printf("  je .L.end.%d\n", seq);
        }
        gen_stmt(node->then);
        if (node->inc) {
            gen_expr(node->inc);
            printf("  pop %%rax\n");
        }
        printf("  jmp .L.begin.%d\n", seq);
        printf(".L.end.%d:\n", seq);
        return;
    }

    default:
        error("unknown statement kind");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        error("usage: %s [--target=linux|windows] <program>", argv[0]);
    }

    int arg_idx = 1;
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "--") == 0) {
            arg_idx++;
            break;
        }

        if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            printf("Usage: %s [options] <program>\n", argv[0]);
            printf("Options:\n");
            printf("  --target=linux    Emit assembly for x86-64 Linux (System V AMD64 ABI)\n");
            printf("  --target=windows  Emit assembly for x86-64 Windows (Microsoft x64 ABI)\n");
            printf("  -h, --help        Show this help message\n");
            return 0;
        }

        if (strncmp(argv[arg_idx], "--target=", 9) == 0) {
            char *target = argv[arg_idx] + 9;
            if (strcmp(target, "linux") == 0) {
                target_os = TARGET_LINUX;
            } else if (strcmp(target, "windows") == 0) {
                target_os = TARGET_WINDOWS;
            } else {
                error("unknown target: %s (supported: linux, windows)", target);
            }
            arg_idx++;
            continue;
        }

        if (strncmp(argv[arg_idx], "--", 2) == 0) {
            error("unknown option: %s", argv[arg_idx]);
        }

        break;
    }

    if (arg_idx >= argc) {
        error("expected program argument");
    }

    user_input = argv[arg_idx];
    token = tokenize(user_input);

    // Parse all statements
    Node *stmts[1000];
    int nstmts = 0;
    while (!at_eof()) {
        if (nstmts >= 1000)
            error("too many statements (max 1000)");
        stmts[nstmts++] = stmt();
    }

    // Count locals and compute stack size (round up to 16-byte alignment)
    int nlocals = 0;
    for (Var *v = locals; v; v = v->next)
        nlocals++;
    int stack_size = ((nlocals * 8) + 15) & ~15; // align to 16 bytes

    // Function prologue
    printf(".globl main\n");
    printf("main:\n");
    printf("  push %%rbp\n");
    printf("  mov %%rsp, %%rbp\n");
    if (stack_size > 0)
        printf("  sub $%d, %%rsp\n", stack_size);

    // Emit code for each statement
    for (int i = 0; i < nstmts; i++)
        gen_stmt(stmts[i]);

    // Function epilogue
    printf(".L.return:\n");
    printf("  mov %%rbp, %%rsp\n");
    printf("  pop %%rbp\n");
    printf("  ret\n");

    // Clear executable stack flag for GNU ld on Linux
    if (target_os == TARGET_LINUX) {
        printf(".section .note.GNU-stack,\"\",@progbits\n");
    }
    return 0;
}
