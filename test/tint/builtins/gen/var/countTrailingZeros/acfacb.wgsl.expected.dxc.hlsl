int3 tint_count_trailing_zeros(int3 v) {
  uint3 x = uint3(v);
  const uint3 b16 = (bool3((x & uint3(65535u, 65535u, 65535u))) ? uint3(0u, 0u, 0u) : uint3(16u, 16u, 16u));
  x = (x >> b16);
  const uint3 b8 = (bool3((x & uint3(255u, 255u, 255u))) ? uint3(0u, 0u, 0u) : uint3(8u, 8u, 8u));
  x = (x >> b8);
  const uint3 b4 = (bool3((x & uint3(15u, 15u, 15u))) ? uint3(0u, 0u, 0u) : uint3(4u, 4u, 4u));
  x = (x >> b4);
  const uint3 b2 = (bool3((x & uint3(3u, 3u, 3u))) ? uint3(0u, 0u, 0u) : uint3(2u, 2u, 2u));
  x = (x >> b2);
  const uint3 b1 = (bool3((x & uint3(1u, 1u, 1u))) ? uint3(0u, 0u, 0u) : uint3(1u, 1u, 1u));
  const uint3 is_zero = ((x == uint3(0u, 0u, 0u)) ? uint3(1u, 1u, 1u) : uint3(0u, 0u, 0u));
  return int3((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void countTrailingZeros_acfacb() {
  int3 arg_0 = int3(1, 1, 1);
  int3 res = tint_count_trailing_zeros(arg_0);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countTrailingZeros_acfacb();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countTrailingZeros_acfacb();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countTrailingZeros_acfacb();
  return;
}
