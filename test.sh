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

echo OK
