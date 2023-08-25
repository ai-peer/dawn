RWByteAddressBuffer prevent_dce : register(u0, space2);

void distance_892a5d() {
  vector<float16_t, 2> arg_0 = vector<float16_t, 2>(float16_t(1.0h), float16_t(1.0h));
  vector<float16_t, 2> arg_1 = vector<float16_t, 2>(float16_t(1.0h), float16_t(1.0h));
  float16_t res = distance(arg_0, arg_1);
  prevent_dce.Store<float16_t>(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  distance_892a5d();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  distance_892a5d();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  distance_892a5d();
  return;
}
