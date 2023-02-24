struct InnerS {
  int v;
};

cbuffer cbuffer_uniforms : register(b4, space1) {
  uint4 uniforms[1];
};
RWByteAddressBuffer s1 : register(u0, space0);

void tint_symbol_1(uint offset, InnerS value) {
  s1.Store((offset + 0u), asuint(value.v));
}

[numthreads(1, 1, 1)]
void main() {
  InnerS v = (InnerS)0;
  tint_symbol_1((4u * uniforms[0].x), v);
  return;
}
