void clamp_8b1eaa() {
  int3 res = int3(1, 1, 1);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  clamp_8b1eaa();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  clamp_8b1eaa();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  clamp_8b1eaa();
  return;
}
