uint3 tint_mod(uint lhs, uint3 rhs) {
  const uint3 l = uint3((lhs).xxx);
  return (l % ((rhs == uint3(0u, 0u, 0u)) ? uint3(1u, 1u, 1u) : rhs));
}

[numthreads(1, 1, 1)]
void f() {
  uint a = 4u;
  uint3 b = uint3(0u, 2u, 0u);
  const uint3 r = tint_mod(a, (b + b));
  return;
}
