uint3 tint_div(uint3 lhs, uint rhs) {
  const uint3 r = uint3((rhs).xxx);
  return (lhs / ((r == uint3(0u, 0u, 0u)) ? uint3(1u, 1u, 1u) : r));
}

[numthreads(1, 1, 1)]
void f() {
  uint3 a = uint3(1u, 2u, 3u);
  uint b = 0u;
  const uint3 r = tint_div(a, (b + b));
  return;
}
