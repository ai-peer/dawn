struct S {
  float f;
};

ByteAddressBuffer tint_symbol : register(t0, space0);
RWByteAddressBuffer tint_symbol_1 : register(u1, space0);

void tint_symbol_2(uint offset, S value) {
  tint_symbol_1.Store((offset + 0u), asuint(value.f));
}

S tint_symbol_4(uint offset) {
  const S tint_symbol_6 = {asfloat(tint_symbol.Load((offset + 0u)))};
  return tint_symbol_6;
}

[numthreads(1, 1, 1)]
void main() {
  tint_symbol_2(0u, tint_symbol_4(0u));
  return;
}
