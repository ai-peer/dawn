struct S {
  float4 a;
  int b;
};

RWByteAddressBuffer sb : register(u0, space0);

S tint_symbol(uint offset) {
  const S tint_symbol_3 = {asfloat(sb.Load4((offset + 0u))), asint(sb.Load((offset + 16u)))};
  return tint_symbol_3;
}

void main_1() {
  const S x_18 = tint_symbol(32u);
  return;
}

[numthreads(1, 1, 1)]
void main() {
  main_1();
  return;
}
