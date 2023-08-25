RWByteAddressBuffer prevent_dce : register(u0, space2);

void countTrailingZeros_8ed26f() {
  uint3 res = uint3(0u, 0u, 0u);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countTrailingZeros_8ed26f();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countTrailingZeros_8ed26f();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countTrailingZeros_8ed26f();
  return;
}
