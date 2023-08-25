void tanh_6289fd() {
  float3 res = float3(0.76159417629241943359f, 0.76159417629241943359f, 0.76159417629241943359f);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  tanh_6289fd();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  tanh_6289fd();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  tanh_6289fd();
  return;
}
