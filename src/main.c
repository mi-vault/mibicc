#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Token kind
typedef enum {
    TK_RESERVED, // Keywords or punctuators (+, -)
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

// Consumes the current token if it matches op
static bool consume(char op) {
    if (token->kind != TK_RESERVED || token->loc[0] != op)
        return false;
    token = token->next;
    return true;
}

// Ensures that the current token is op
static void expect(char op) {
    if (token->kind != TK_RESERVED || token->loc[0] != op)
        error_at(token->loc, "expected '%c'", op);
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

        // Operators (+, -)
        if (*p == '+' || *p == '-') {
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

int main(int argc, char **argv) {
    if (argc != 2) {
        error("%s: invalid number of arguments", argv[0]);
    }

    user_input = argv[1];
    token = tokenize(user_input);

    printf(".globl main\n");
    printf("main:\n");

    // The first token must be a number
    printf("  mov $%ld, %%rax\n", expect_number());

    // Generate assembly while tokens remain
    while (!at_eof()) {
        if (consume('+')) {
            printf("  add $%ld, %%rax\n", expect_number());
            continue;
        }

        expect('-');
        printf("  sub $%ld, %%rax\n", expect_number());
    }

    printf("  ret\n");
    return 0;
}
