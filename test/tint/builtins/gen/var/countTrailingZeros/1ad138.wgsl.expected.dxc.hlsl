uint2 tint_count_trailing_zeros(uint2 v) {
  uint2 x = uint2(v);
  const uint2 b16 = (bool2((x & uint2(65535u, 65535u))) ? uint2(0u, 0u) : uint2(16u, 16u));
  x = (x >> b16);
  const uint2 b8 = (bool2((x & uint2(255u, 255u))) ? uint2(0u, 0u) : uint2(8u, 8u));
  x = (x >> b8);
  const uint2 b4 = (bool2((x & uint2(15u, 15u))) ? uint2(0u, 0u) : uint2(4u, 4u));
  x = (x >> b4);
  const uint2 b2 = (bool2((x & uint2(3u, 3u))) ? uint2(0u, 0u) : uint2(2u, 2u));
  x = (x >> b2);
  const uint2 b1 = (bool2((x & uint2(1u, 1u))) ? uint2(0u, 0u) : uint2(1u, 1u));
  const uint2 is_zero = ((x == uint2(0u, 0u)) ? uint2(1u, 1u) : uint2(0u, 0u));
  return uint2((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void countTrailingZeros_1ad138() {
  uint2 arg_0 = uint2(1u, 1u);
  uint2 res = tint_count_trailing_zeros(arg_0);
  prevent_dce.Store2(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countTrailingZeros_1ad138();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countTrailingZeros_1ad138();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countTrailingZeros_1ad138();
  return;
}
