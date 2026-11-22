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
    TK_RESERVED, // Keywords or punctuators (+, -, *, /, (, ))
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
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_EQ,  // ==
    ND_NE,  // !=
    ND_LT,  // <
    ND_LE,  // <=
    ND_NEG, // Unary -
    ND_NUM, // Integer
} NodeKind;

// AST Node type
typedef struct Node Node;

struct Node {
    NodeKind kind; // Node kind
    Node *lhs;     // Left-hand side
    Node *rhs;     // Right-hand side
    long val;      // Used if kind == ND_NUM
};

// Input program string
static char *user_input;

// Current token
static Token *token;

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

        // Single-character punctuators (+, -, *, /, (, ), <, >)
        if (strchr("+-*/()<>=;", *p)) {
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

        error_at(p, "invalid token");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
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

// Parser forward declarations
static Node *expr(void);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *mul(void);
static Node *unary(void);
static Node *primary(void);

// primary = "(" expr ")" | num
static Node *primary(void) {
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    return new_node_num(expect_number());
}

// unary = ("+" | "-") unary | primary
static Node *unary(void) {
    if (consume("+"))
        return unary();
    if (consume("-"))
        return new_node(ND_NEG, unary(), NULL);
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

// expr = equality
static Node *expr(void) {
    return equality();
}

// Code generator: evaluates AST using a stack machine
static void gen(Node *node) {
    if (node->kind == ND_NUM) {
        printf("  push $%ld\n", node->val);
        return;
    }

    if (node->kind == ND_NEG) {
        gen(node->lhs);
        printf("  pop %%rax\n");
        printf("  neg %%rax\n");
        printf("  push %%rax\n");
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

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
        error("unknown node kind");
    }

    printf("  push %%rax\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        error("usage: %s [--target=linux|windows] <expression>", argv[0]);
    }

    int arg_idx = 1;
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "--") == 0) {
            arg_idx++;
            break;
        }

        if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            printf("Usage: %s [options] <expression>\n", argv[0]);
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
        error("expected expression argument");
    }

    user_input = argv[arg_idx];
    token = tokenize(user_input);

    Node *node = expr();

    if (!at_eof()) {
        error_at(token->loc, "stray token after expression");
    }

    printf(".globl main\n");
    printf("main:\n");

    // Traverse the AST to emit assembly
    gen(node);

    // The result of the expression is on top of the stack.
    // Pop it into %rax to be the return value of main.
    printf("  pop %%rax\n");
    printf("  ret\n");
    return 0;
}
