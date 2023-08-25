uint4 tint_first_leading_bit(uint4 v) {
  uint4 x = v;
  const uint4 b16 = (bool4((x & uint4(4294901760u, 4294901760u, 4294901760u, 4294901760u))) ? uint4(16u, 16u, 16u, 16u) : uint4(0u, 0u, 0u, 0u));
  x = (x >> b16);
  const uint4 b8 = (bool4((x & uint4(65280u, 65280u, 65280u, 65280u))) ? uint4(8u, 8u, 8u, 8u) : uint4(0u, 0u, 0u, 0u));
  x = (x >> b8);
  const uint4 b4 = (bool4((x & uint4(240u, 240u, 240u, 240u))) ? uint4(4u, 4u, 4u, 4u) : uint4(0u, 0u, 0u, 0u));
  x = (x >> b4);
  const uint4 b2 = (bool4((x & uint4(12u, 12u, 12u, 12u))) ? uint4(2u, 2u, 2u, 2u) : uint4(0u, 0u, 0u, 0u));
  x = (x >> b2);
  const uint4 b1 = (bool4((x & uint4(2u, 2u, 2u, 2u))) ? uint4(1u, 1u, 1u, 1u) : uint4(0u, 0u, 0u, 0u));
  const uint4 is_zero = ((x == uint4(0u, 0u, 0u, 0u)) ? uint4(4294967295u, 4294967295u, 4294967295u, 4294967295u) : uint4(0u, 0u, 0u, 0u));
  return uint4((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void firstLeadingBit_000ff3() {
  uint4 arg_0 = uint4(1u, 1u, 1u, 1u);
  uint4 res = tint_first_leading_bit(arg_0);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  firstLeadingBit_000ff3();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  firstLeadingBit_000ff3();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  firstLeadingBit_000ff3();
  return;
}
