struct S {
  float4 a;
  int b;
};

ByteAddressBuffer sb : register(t0, space0);

S tint_symbol(uint offset) {
  const S tint_symbol_3 = {asfloat(sb.Load4((offset + 0u))), asint(sb.Load((offset + 16u)))};
  return tint_symbol_3;
}

[numthreads(1, 1, 1)]
void main() {
  const S x = tint_symbol(32u);
  return;
}
