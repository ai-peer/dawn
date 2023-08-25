RWByteAddressBuffer prevent_dce : register(u0, space2);

void atan2_ae713e() {
  float4 arg_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 arg_1 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 res = atan2(arg_0, arg_1);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  atan2_ae713e();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  atan2_ae713e();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atan2_ae713e();
  return;
}
