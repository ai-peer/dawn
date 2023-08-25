struct modf_result_vec3_f32 {
  float3 fract;
  float3 whole;
};
void modf_68d8ee() {
  modf_result_vec3_f32 res = {float3(-0.5f, -0.5f, -0.5f), float3(-1.0f, -1.0f, -1.0f)};
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  modf_68d8ee();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  modf_68d8ee();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  modf_68d8ee();
  return;
}
