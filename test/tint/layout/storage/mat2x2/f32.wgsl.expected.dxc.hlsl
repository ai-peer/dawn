RWByteAddressBuffer ssbo : register(u0, space0);

float2x2 tint_symbol(uint offset) {
  return float2x2(asfloat(ssbo.Load2((offset + 0u))), asfloat(ssbo.Load2((offset + 8u))));
}

void tint_symbol_2(uint offset, float2x2 value) {
  ssbo.Store2((offset + 0u), asuint(value[0u]));
  ssbo.Store2((offset + 8u), asuint(value[1u]));
}

[numthreads(1, 1, 1)]
void f() {
  const float2x2 v = tint_symbol(0u);
  tint_symbol_2(0u, v);
  return;
}
