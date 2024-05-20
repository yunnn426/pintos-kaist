#include <stdio.h>
#include <stdint.h>
#include "threads/fixed_point.h";

// fixed_point 간 덧셈
int fixed_add(int x, int y) { 
    return x + y;
}
// fixed_point 간 뺄셈
int fixed_sub(int x, int y) {
    return x - y;
}

// fixed_point + 정수 
int fixed_add_int(int x, int n) {
    return x + n * F;
}

// fixed_point - 정수
int fixed_sub_int(int x, int n) {
    return x - n * F;
}

// fixed_point 끼리 곱하기
int fixed_mul(int x, int y) {
    return ((int64_t) x) * y / F;
}

// fixed_point 끼리 나누기
int fixed_div(int x, int y) {
    return ((int64_t) x) * F / y;
}

// 정수를 fixed_point로
int int_to_fixed(int n) {
    return n * F;
}

// fixed_point * 정수
int fixed_mul_int (int x, int n) {
  return x * n;
}

// fixed_point / 정수
int fixed_div_int (int x, int n) {
  return x / n;
}

// fixed_point를 정수로 바꾸되, 반올림처리
int fixed_to_int_round(int x) {
    if (x >= 0) {
        return (x + F / 2) / F;
    } else {
        return (x - F / 2) / F;
    }
}

// fixed_point를 정수 바꾸되, 내림
int fixed_to_int_trunc(int x) {
    return x / F;
}