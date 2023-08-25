int3 tint_div(int3 lhs, int rhs) {
  const int3 r = int3((rhs).xxx);
  return (lhs / (((r == int3(0, 0, 0)) | ((lhs == int3(-2147483648, -2147483648, -2147483648)) & (r == int3(-1, -1, -1)))) ? int3(1, 1, 1) : r));
}

[numthreads(1, 1, 1)]
void f() {
  const int3 a = int3(1, 2, 3);
  const int b = 0;
  const int3 r = tint_div(a, b);
  return;
}
