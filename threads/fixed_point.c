#include <stdio.h>
#include <stdint.h>
#include "threads/fixed_point.h";

// 둘다 소수의 덧셈
int fixed_add(int x, int y) { 
    return x + y;
}
// 둘다 소수의 뺄셈
// 0000000  1 1 1 . 1 0 1 000000000
// 32비트를 이렇게 쓰겠다. 42344 -> 소수점처럼 
int fixed_sub(int x, int y) {
    return x - y;
}

// 소수 + 정수 
int fixed_add_int(int x, int n) {
    return x + n * F;
}

// 소수 - 정수
int fixed_sub_int(int x, int n) {
    return x - n * F;
}

// 소수끼리 곱하기
int fixed_mul(int x, int y) {
    return ((int64_t) x) * y / F;
}

// 소수끼리 나누기
int fixed_div(int x, int y) {
    return ((int64_t) x) * F / y;
}

// 정수를 소수로
int int_to_fixed(int n) {
    return n * F;
}

int fixed_mul_int (int x, int n) {
  return x * n;
}

// 
int fixed_div_int (int x, int n) {
  return x / n;
}

// 소수를 정수로 반올림처리
int fixed_to_int_round(int x) {
    if (x >= 0) {
        return (x + F / 2) / F;
    } else {
        return (x - F / 2) / F;
    }
}

// 소수를 정수 바꾸되, 내림
int fixed_to_int_trunc(int x) {
    return x / F;
}