int2 tint_count_leading_zeros(int2 v) {
  uint2 x = uint2(v);
  const uint2 b16 = ((x <= uint2(65535u, 65535u)) ? uint2(16u, 16u) : uint2(0u, 0u));
  x = (x << b16);
  const uint2 b8 = ((x <= uint2(16777215u, 16777215u)) ? uint2(8u, 8u) : uint2(0u, 0u));
  x = (x << b8);
  const uint2 b4 = ((x <= uint2(268435455u, 268435455u)) ? uint2(4u, 4u) : uint2(0u, 0u));
  x = (x << b4);
  const uint2 b2 = ((x <= uint2(1073741823u, 1073741823u)) ? uint2(2u, 2u) : uint2(0u, 0u));
  x = (x << b2);
  const uint2 b1 = ((x <= uint2(2147483647u, 2147483647u)) ? uint2(1u, 1u) : uint2(0u, 0u));
  const uint2 is_zero = ((x == uint2(0u, 0u)) ? uint2(1u, 1u) : uint2(0u, 0u));
  return int2((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void countLeadingZeros_858d40() {
  int2 arg_0 = int2(1, 1);
  int2 res = tint_count_leading_zeros(arg_0);
  prevent_dce.Store2(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countLeadingZeros_858d40();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countLeadingZeros_858d40();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countLeadingZeros_858d40();
  return;
}
