#define F 1 << 14

int int_to_fixed(int n);
int fixed_to_int_trunc(int x);
int fixed_to_int_round(int x);
int fixed_add(int x, int y);
int fixed_sub(int x, int y);
int fixed_add_int(int x, int n);
int fixed_sub_int(int x, int n);
int fixed_mul(int x, int y);
int fixed_mul_int(int x, int n);
int fixed_div(int x, int y);
int fixed_div_int(int x, int n);