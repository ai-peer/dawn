int3 tint_mod(int3 lhs, int rhs) {
  const int3 r = int3((rhs).xxx);
  const int3 rhs_or_one = (((r == int3(0, 0, 0)) | ((lhs == int3(-2147483648, -2147483648, -2147483648)) & (r == int3(-1, -1, -1)))) ? int3(1, 1, 1) : r);
  if (any(((uint3((lhs | rhs_or_one)) & uint3(2147483648u, 2147483648u, 2147483648u)) != uint3(0u, 0u, 0u)))) {
    return (lhs - ((lhs / rhs_or_one) * rhs_or_one));
  } else {
    return (lhs % rhs_or_one);
  }
}

[numthreads(1, 1, 1)]
void f() {
  int3 a = int3(1, 2, 3);
  int b = 0;
  const int3 r = tint_mod(a, (b + b));
  return;
}
