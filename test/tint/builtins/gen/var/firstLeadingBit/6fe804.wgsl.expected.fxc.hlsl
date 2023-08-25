uint2 tint_first_leading_bit(uint2 v) {
  uint2 x = v;
  const uint2 b16 = (bool2((x & uint2(4294901760u, 4294901760u))) ? uint2(16u, 16u) : uint2(0u, 0u));
  x = (x >> b16);
  const uint2 b8 = (bool2((x & uint2(65280u, 65280u))) ? uint2(8u, 8u) : uint2(0u, 0u));
  x = (x >> b8);
  const uint2 b4 = (bool2((x & uint2(240u, 240u))) ? uint2(4u, 4u) : uint2(0u, 0u));
  x = (x >> b4);
  const uint2 b2 = (bool2((x & uint2(12u, 12u))) ? uint2(2u, 2u) : uint2(0u, 0u));
  x = (x >> b2);
  const uint2 b1 = (bool2((x & uint2(2u, 2u))) ? uint2(1u, 1u) : uint2(0u, 0u));
  const uint2 is_zero = ((x == uint2(0u, 0u)) ? uint2(4294967295u, 4294967295u) : uint2(0u, 0u));
  return uint2((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void firstLeadingBit_6fe804() {
  uint2 arg_0 = uint2(1u, 1u);
  uint2 res = tint_first_leading_bit(arg_0);
  prevent_dce.Store2(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  firstLeadingBit_6fe804();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  firstLeadingBit_6fe804();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  firstLeadingBit_6fe804();
  return;
}
