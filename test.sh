#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./mibicc "$input" > tmp.s
  gcc -static -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert_target() {
  expected="$1"
  target="$2"
  input="$3"

  ./mibicc "--target=$target" "$input" > tmp.s
  gcc -static -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "[$target] $input => $actual"
  else
    echo "[$target] $input => $expected expected, but got $actual"
    exit 1
  fi
}

# Basic constants
assert 0  "0;"
assert 42 "42;"

# Simple arithmetic without spaces
assert 21 "5+20-4;"
assert 41 "12+34-5;"

# Arithmetic with whitespace
assert 42 "  42  ;"
assert 21 "5 + 20 - 4;"
assert 41 " 12 + 34 - 5 ;"
assert 150 " 100 +   200  -  150 ;"
assert 5  "1 + 2 + 3 + 4 - 5;"
assert 10 "0 + 10;"

# Operator precedence (*, /)
assert 47 "5+6*7;"
assert 15 "5*(9-6);"
assert 4  "(3+5)/2;"
assert 29 "10 + 20 / 2 * 2 - 1;"
assert 10 "2 * 5;"
assert 3  "12 / 4;"
assert 26 "2 * (3 + (4 * 2) + 2);"
assert 4  "((2 + 2));"
assert 2  "(4 - 2) * (3 - 2);"
assert 100 "(10 * 10);"
assert 1  "100 / 10 / 10;"

# Explicit target flags
assert_target 42 "linux"   "40 + 2;"
assert_target 42 "windows" "6 * 7;"

# Unary operators (+, -)
assert 10 "-10 + 20;"
assert 10 "- -10;"
assert 10 "- - +10;"
assert 42 "+42;"
assert 0  "+0;"
assert 15 "-3 * -5;"
assert 4  "-(3 + 5) + 12;"
assert 20 "-(-10 * 2);"
assert 5  "- - -5 + 10;"
assert 15 "- - - -5 + 10;"
assert 10 "+ +10;"
assert 1  "(-10 + 20) / 10;"
assert_target 10 "linux"   "-10 + 20;"
assert_target 15 "windows" "-3 * -5;"

# Equality and relational operators (==, !=, <, <=, >, >=)
assert 0 "0 == 1;"
assert 1 "42 == 42;"
assert 1 "0 != 1;"
assert 0 "42 != 42;"

assert 1 "0 < 1;"
assert 0 "1 < 1;"
assert 0 "2 < 1;"
assert 1 "0 <= 1;"
assert 1 "1 <= 1;"
assert 0 "2 <= 1;"

assert 1 "1 > 0;"
assert 0 "1 > 1;"
assert 0 "1 > 2;"
assert 1 "1 >= 0;"
assert 1 "1 >= 1;"
assert 0 "1 >= 2;"

# Precedence between arithmetic and comparison
assert 1 "1 + 2 == 3;"
assert 0 "1 + 2 == 4;"
assert 1 "1 + 2 < 2 + 2;"
assert 0 "1 + 2 < 2 + 1;"
assert 1 "2 * 3 > 5;"
assert 0 "2 * 3 > 6;"
assert 2 "(1 == 1) + 1;"
assert 5 "(1 != 2) * 5;"
assert 1 "1 == 2 == 0;"

# Target ABI comparisons
assert_target 1 "linux"   "5 <= 10;"
assert_target 1 "windows" "10 >= 5;"

# Local variables and assignment (Step 7)
assert 3  "a=3;"
assert 42 "a=42;"
assert 5  "a=5;a;"
assert 7  "a=3;b=4;a+b;"
assert 15 "a=3;b=5;a*b;"
assert 1  "a=10;b=10;a==b;"
assert 0  "a=10;b=20;a==b;"
assert 42 "a=6;b=7;a*b;"
assert 2  "a=10;a=a/5;a;"
assert 10 "a=3;b=a+7;b;"

# Chained assignment
assert 5  "a=b=5;a;"
assert 5  "a=b=5;b;"

# Multi-statement: intermediate results discarded, last is return value
assert 3  "1+1;2+2;1+2;"
assert 39 "x=3;y=x;x=9;x=y;y=9;10*x+y;"

# Return statement & multi-letter variables (Step 8)
assert 0  "return 0;"
assert 42 "return 42;"
assert 10 "return 5 + 5;"
assert 21 "return 5 + 20 - 4;"
assert 3  "a = 3; return a;"
assert 7  "a = 3; b = 4; return a + b;"
assert 42 "foo = 6; bar = 7; return foo * bar;"
assert 10 "foo = 10; foo;"
assert 14 "foo = 3; bar = 4; return foo + bar * 2 + 3;"
assert 50 "result_val = 25; return result_val * 2;"
assert 2  "x1 = 1; x2 = 2; return x2;"
assert 3  "foo_bar = 3; return foo_bar;"
assert 1  "return 1; 2;"
assert 5  "return 5; return 10;"
assert 10 "a = 10; return a; a = 20; return a;"
assert 3  "x = 1; y = 2; return x + y; x = 10; return x;"
assert_target 42  "linux"   "return 40 + 2;"
assert_target 42  "windows" "return 6 * 7;"
assert_target 33  "linux"   "val = 11; return val * 3;"
assert_target 100 "windows" "hundred = 10; return hundred * hundred;"

# Control flow: if, else, while, for, blocks (Step 9)
assert 3  "if (0) return 2; return 3;"
assert 3  "if (1-1) return 2; return 3;"
assert 2  "if (1) return 2; return 3;"
assert 2  "if (2 > 1) return 2; return 3;"
assert 3  "if (0) return 2; else return 3;"
assert 2  "if (1) return 2; else return 3;"
assert 2  "if (1 > 0) return 2; else return 3;"
assert 3  "if (1 < 0) return 2; else return 3;"
assert 42 "val = 10; if (val == 10) val = 42; else val = 0; return val;"
assert 2  "if (1) if (0) return 1; else return 2;"
assert 3  "if (0) if (1) return 1; else return 2; else return 3;"

# While loop
assert 10 "i = 0; while (i < 10) i = i + 1; return i;"
assert 55 "i = 1; sum = 0; while (i <= 10) { sum = sum + i; i = i + 1; } return sum;"

# For loop
assert 55 "sum = 0; for (i = 1; i <= 10; i = i + 1) sum = sum + i; return sum;"
assert 45 "sum = 0; for (i = 0; i < 10; i = i + 1) sum = sum + i; return sum;"
assert 10 "i = 0; for (; i < 10; i = i + 1) ; return i;"
assert 10 "i = 0; for (; i < 10;) i = i + 1; return i;"
assert 5  "i = 0; for (;;) { i = i + 1; if (i == 5) return i; }"

# Block statements & empty statements
assert 3  "{ 1; { 2; } return 3; }"
assert 30 "a = 10; { b = 20; a = a + b; } return a;"
assert 15 "x = 0; if (1) { x = 10; x = x + 5; } return x;"
assert 20 "x = 0; if (0) { x = 10; } else { x = 20; } return x;"
assert 42 ";;; return 42;"

# Target ABI checks with control flow
assert_target 55 "linux"   "sum = 0; for (i = 1; i <= 10; i = i + 1) sum = sum + i; return sum;"
assert_target 55 "windows" "sum = 0; for (i = 1; i <= 10; i = i + 1) sum = sum + i; return sum;"
assert_target 1  "linux"   "if (1) return 1; else return 2;"
assert_target 2  "windows" "if (0) return 1; else return 2;"

echo OK
