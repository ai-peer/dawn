int3 tint_mod(int lhs, int3 rhs) {
  const int3 l = int3((lhs).xxx);
  const int3 rhs_or_one = (((rhs == int3(0, 0, 0)) | ((l == int3(-2147483648, -2147483648, -2147483648)) & (rhs == int3(-1, -1, -1)))) ? int3(1, 1, 1) : rhs);
  if (any(((uint3((l | rhs_or_one)) & uint3(2147483648u, 2147483648u, 2147483648u)) != uint3(0u, 0u, 0u)))) {
    return (l - ((l / rhs_or_one) * rhs_or_one));
  } else {
    return (l % rhs_or_one);
  }
}

[numthreads(1, 1, 1)]
void f() {
  int a = 4;
  int3 b = int3(0, 2, 0);
  const int3 r = tint_mod(a, b);
  return;
}
