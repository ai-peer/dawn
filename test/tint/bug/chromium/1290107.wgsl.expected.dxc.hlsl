cbuffer cbuffer_tint_symbol_1 : register(b30, space0) {
  uint4 tint_symbol_1[1];
};

ByteAddressBuffer arr : register(t0, space0);

[numthreads(1, 1, 1)]
void main() {
  const uint len = (tint_symbol_1[0].x / 4u);
  return;
}
