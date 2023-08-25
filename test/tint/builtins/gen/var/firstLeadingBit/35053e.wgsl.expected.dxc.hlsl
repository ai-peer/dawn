int3 tint_first_leading_bit(int3 v) {
  uint3 x = ((v < int3(0, 0, 0)) ? uint3(~(v)) : uint3(v));
  const uint3 b16 = (bool3((x & uint3(4294901760u, 4294901760u, 4294901760u))) ? uint3(16u, 16u, 16u) : uint3(0u, 0u, 0u));
  x = (x >> b16);
  const uint3 b8 = (bool3((x & uint3(65280u, 65280u, 65280u))) ? uint3(8u, 8u, 8u) : uint3(0u, 0u, 0u));
  x = (x >> b8);
  const uint3 b4 = (bool3((x & uint3(240u, 240u, 240u))) ? uint3(4u, 4u, 4u) : uint3(0u, 0u, 0u));
  x = (x >> b4);
  const uint3 b2 = (bool3((x & uint3(12u, 12u, 12u))) ? uint3(2u, 2u, 2u) : uint3(0u, 0u, 0u));
  x = (x >> b2);
  const uint3 b1 = (bool3((x & uint3(2u, 2u, 2u))) ? uint3(1u, 1u, 1u) : uint3(0u, 0u, 0u));
  const uint3 is_zero = ((x == uint3(0u, 0u, 0u)) ? uint3(4294967295u, 4294967295u, 4294967295u) : uint3(0u, 0u, 0u));
  return int3((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void firstLeadingBit_35053e() {
  int3 arg_0 = int3(1, 1, 1);
  int3 res = tint_first_leading_bit(arg_0);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  firstLeadingBit_35053e();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  firstLeadingBit_35053e();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  firstLeadingBit_35053e();
  return;
}
