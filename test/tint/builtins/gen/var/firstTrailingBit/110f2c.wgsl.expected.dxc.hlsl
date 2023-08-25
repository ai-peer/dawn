uint4 tint_first_trailing_bit(uint4 v) {
  uint4 x = uint4(v);
  const uint4 b16 = (bool4((x & uint4(65535u, 65535u, 65535u, 65535u))) ? uint4(0u, 0u, 0u, 0u) : uint4(16u, 16u, 16u, 16u));
  x = (x >> b16);
  const uint4 b8 = (bool4((x & uint4(255u, 255u, 255u, 255u))) ? uint4(0u, 0u, 0u, 0u) : uint4(8u, 8u, 8u, 8u));
  x = (x >> b8);
  const uint4 b4 = (bool4((x & uint4(15u, 15u, 15u, 15u))) ? uint4(0u, 0u, 0u, 0u) : uint4(4u, 4u, 4u, 4u));
  x = (x >> b4);
  const uint4 b2 = (bool4((x & uint4(3u, 3u, 3u, 3u))) ? uint4(0u, 0u, 0u, 0u) : uint4(2u, 2u, 2u, 2u));
  x = (x >> b2);
  const uint4 b1 = (bool4((x & uint4(1u, 1u, 1u, 1u))) ? uint4(0u, 0u, 0u, 0u) : uint4(1u, 1u, 1u, 1u));
  const uint4 is_zero = ((x == uint4(0u, 0u, 0u, 0u)) ? uint4(4294967295u, 4294967295u, 4294967295u, 4294967295u) : uint4(0u, 0u, 0u, 0u));
  return uint4((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void firstTrailingBit_110f2c() {
  uint4 arg_0 = uint4(1u, 1u, 1u, 1u);
  uint4 res = tint_first_trailing_bit(arg_0);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  firstTrailingBit_110f2c();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  firstTrailingBit_110f2c();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  firstTrailingBit_110f2c();
  return;
}
