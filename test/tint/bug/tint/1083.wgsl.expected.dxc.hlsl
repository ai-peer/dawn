int tint_div(int lhs, int rhs) {
  const int rhs_or_one = (((rhs == 0) | ((lhs == -2147483648) & (rhs == -1))) ? 1 : rhs);
  return (lhs / rhs_or_one);
}

[numthreads(1, 1, 1)]
void f() {
  const int a = 1;
  const int b = 0;
  const int c = tint_div(a, b);
  return;
}
