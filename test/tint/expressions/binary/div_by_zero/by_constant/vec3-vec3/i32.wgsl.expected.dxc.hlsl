int3 tint_div(int3 lhs, int3 rhs) {
  return (lhs / (((rhs == int3(0, 0, 0)) | ((lhs == int3(-2147483648, -2147483648, -2147483648)) & (rhs == int3(-1, -1, -1)))) ? int3(1, 1, 1) : rhs));
}

[numthreads(1, 1, 1)]
void f() {
  const int3 a = int3(1, 2, 3);
  const int3 b = int3(0, 5, 0);
  const int3 r = tint_div(a, b);
  return;
}
