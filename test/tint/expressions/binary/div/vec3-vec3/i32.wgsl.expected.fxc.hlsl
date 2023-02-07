int3 tint_div(int3 lhs, int3 rhs) {
  const int3 rhs_or_one = (((rhs == (0).xxx) | ((lhs == (-2147483648).xxx) & (rhs == (-1).xxx))) ? (1).xxx : rhs);
  return (lhs / rhs_or_one);
}

[numthreads(1, 1, 1)]
void f() {
  const int3 a = int3(1, 2, 3);
  const int3 b = int3(4, 5, 6);
  const int3 r = tint_div(a, b);
  return;
}
