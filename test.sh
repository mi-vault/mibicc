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
assert 0 0
assert 42 42

# Simple arithmetic without spaces
assert 21 "5+20-4"
assert 41 "12+34-5"

# Arithmetic with whitespace
assert 42 "  42  "
assert 21 "5 + 20 - 4"
assert 41 " 12 + 34 - 5 "
assert 150 " 100 +   200  -  150 "
assert 5 "1 + 2 + 3 + 4 - 5"
assert 10 "0 + 10"

# Operator precedence (*, /)
assert 47 "5+6*7"
assert 15 "5*(9-6)"
assert 4 "(3+5)/2"
assert 29 "10 + 20 / 2 * 2 - 1"
assert 10 "2 * 5"
assert 3 "12 / 4"
assert 26 "2 * (3 + (4 * 2) + 2)"
assert 4 "((2 + 2))"
assert 2 "(4 - 2) * (3 - 2)"
assert 100 "(10 * 10)"
assert 1 "100 / 10 / 10"

# Explicit target flags
assert_target 42 "linux" "40 + 2"
assert_target 42 "windows" "6 * 7"

# Unary operators (+, -)
assert 10 "-10 + 20"
assert 10 "- -10"
assert 10 "- - +10"
assert 42 "+42"
assert 0 "+0"
assert 15 "-3 * -5"
assert 4 "-(3 + 5) + 12"
assert 20 "-(-10 * 2)"
assert 5 "- - -5 + 10"
assert 15 "- - - -5 + 10"
assert 10 "+ +10"
assert 1 "(-10 + 20) / 10"
assert_target 10 "linux" "-10 + 20"
assert_target 15 "windows" "-3 * -5"

echo OK
