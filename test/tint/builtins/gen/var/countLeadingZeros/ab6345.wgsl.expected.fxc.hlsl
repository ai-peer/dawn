uint3 tint_count_leading_zeros(uint3 v) {
  uint3 x = uint3(v);
  const uint3 b16 = ((x <= uint3(65535u, 65535u, 65535u)) ? uint3(16u, 16u, 16u) : uint3(0u, 0u, 0u));
  x = (x << b16);
  const uint3 b8 = ((x <= uint3(16777215u, 16777215u, 16777215u)) ? uint3(8u, 8u, 8u) : uint3(0u, 0u, 0u));
  x = (x << b8);
  const uint3 b4 = ((x <= uint3(268435455u, 268435455u, 268435455u)) ? uint3(4u, 4u, 4u) : uint3(0u, 0u, 0u));
  x = (x << b4);
  const uint3 b2 = ((x <= uint3(1073741823u, 1073741823u, 1073741823u)) ? uint3(2u, 2u, 2u) : uint3(0u, 0u, 0u));
  x = (x << b2);
  const uint3 b1 = ((x <= uint3(2147483647u, 2147483647u, 2147483647u)) ? uint3(1u, 1u, 1u) : uint3(0u, 0u, 0u));
  const uint3 is_zero = ((x == uint3(0u, 0u, 0u)) ? uint3(1u, 1u, 1u) : uint3(0u, 0u, 0u));
  return uint3((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void countLeadingZeros_ab6345() {
  uint3 arg_0 = uint3(1u, 1u, 1u);
  uint3 res = tint_count_leading_zeros(arg_0);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countLeadingZeros_ab6345();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countLeadingZeros_ab6345();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countLeadingZeros_ab6345();
  return;
}
