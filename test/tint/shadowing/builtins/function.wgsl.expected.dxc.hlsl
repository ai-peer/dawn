[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer S : register(u0, space0);

void min_1() {
}

void f(int i) {
  uint tint_symbol_2 = 0u;
  S.GetDimensions(tint_symbol_2);
  const uint tint_symbol_3 = (tint_symbol_2 / 16u);
  min_1();
  S.Store4((16u * min(uint(i), (tint_symbol_3 - 1u))), asuint((1.0f).xxxx));
}
