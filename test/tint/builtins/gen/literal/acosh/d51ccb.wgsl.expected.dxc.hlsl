RWByteAddressBuffer prevent_dce : register(u0, space2);

void acosh_d51ccb() {
  float4 res = float4(1.0f, 1.0f, 1.0f, 1.0f);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  acosh_d51ccb();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  acosh_d51ccb();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  acosh_d51ccb();
  return;
}
