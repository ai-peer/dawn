struct Inner {
  float f;
};
struct S {
  Inner inner;
};

cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[1];
};

Inner tint_symbol_1(uint4 buffer[1], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const Inner tint_symbol_3 = {asfloat(buffer[scalar_offset_index / 4][scalar_offset_index % 4])};
  return tint_symbol_3;
}

S tint_symbol(uint4 buffer[1], uint offset) {
  const S tint_symbol_4 = {tint_symbol_1(buffer, (offset + 0u))};
  return tint_symbol_4;
}

[numthreads(1, 1, 1)]
void main() {
  const S x = tint_symbol(u, 0u);
  return;
}
