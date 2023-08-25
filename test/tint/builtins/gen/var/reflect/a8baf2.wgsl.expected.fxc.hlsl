void reflect_a8baf2() {
  float3 res = float3(-5.0f, -5.0f, -5.0f);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  reflect_a8baf2();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  reflect_a8baf2();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  reflect_a8baf2();
  return;
}
