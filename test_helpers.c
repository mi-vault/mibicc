// Test helper functions for mibicc test suite

int ret42(void) {
    return 42;
}

int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int mul(int a, int b) {
    return a * b;
}

int div2(int a, int b) {
    return a / b;
}

int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

// Windows x64 ABI helpers (compiled with ms_abi attribute on GCC/Clang)
#if defined(__GNUC__) || defined(__clang__)
int __attribute__((ms_abi)) win_ret42(void) {
    return 42;
}

int __attribute__((ms_abi)) win_add(int a, int b) {
    return a + b;
}

int __attribute__((ms_abi)) win_sub(int a, int b) {
    return a - b;
}

int __attribute__((ms_abi)) win_sum4(int a, int b, int c, int d) {
    return a + b + c + d;
}

int __attribute__((ms_abi)) win_sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}
#endif
