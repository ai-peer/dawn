uint tint_mod(uint lhs, uint rhs) {
  return (lhs % ((rhs == 0u) ? 1u : rhs));
}

struct tint_symbol_3 {
  uint3 v : SV_DispatchThreadID;
};

void f_inner(uint3 v) {
  const uint tint_symbol = v.x;
  const uint tint_symbol_1 = tint_mod(v.y, 1u);
  const uint l = (tint_symbol << (tint_symbol_1 & 31u));
}

[numthreads(1, 1, 1)]
void f(tint_symbol_3 tint_symbol_2) {
  f_inner(tint_symbol_2.v);
  return;
}
