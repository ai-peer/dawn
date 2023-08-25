void normalize_584e47() {
  float2 res = float2(0.70710676908493041992f, 0.70710676908493041992f);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  normalize_584e47();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  normalize_584e47();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  normalize_584e47();
  return;
}
