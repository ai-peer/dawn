cbuffer cbuffer_tint_symbol_1 : register(b30, space0) {
  uint4 tint_symbol_1[1];
};

ByteAddressBuffer G : register(t0, space0);

[numthreads(1, 1, 1)]
void main() {
  const uint l1 = ((tint_symbol_1[0].x - 0u) / 4u);
  return;
}
