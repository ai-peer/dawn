struct InnerS {
  int v;
};

cbuffer cbuffer_uniforms : register(b4, space1) {
  uint4 uniforms[1];
};
RWByteAddressBuffer s1 : register(u0, space0);

void tint_symbol_2(RWByteAddressBuffer buffer, uint offset, InnerS value) {
  buffer.Store((offset + 0u), asuint(value.v));
}

[numthreads(1, 1, 1)]
void main() {
  InnerS v = (InnerS)0;
  const uint tint_symbol = uniforms[0].x;
  tint_symbol_2(s1, (4u * tint_symbol), v);
  return;
}
