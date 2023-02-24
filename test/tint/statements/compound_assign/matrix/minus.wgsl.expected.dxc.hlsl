[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer v : register(u0, space0);

void tint_symbol(uint offset, float4x4 value) {
  v.Store4((offset + 0u), asuint(value[0u]));
  v.Store4((offset + 16u), asuint(value[1u]));
  v.Store4((offset + 32u), asuint(value[2u]));
  v.Store4((offset + 48u), asuint(value[3u]));
}

float4x4 tint_symbol_2(uint offset) {
  return float4x4(asfloat(v.Load4((offset + 0u))), asfloat(v.Load4((offset + 16u))), asfloat(v.Load4((offset + 32u))), asfloat(v.Load4((offset + 48u))));
}

void foo() {
  tint_symbol(0u, (tint_symbol_2(0u) - float4x4((0.0f).xxxx, (0.0f).xxxx, (0.0f).xxxx, (0.0f).xxxx)));
}
