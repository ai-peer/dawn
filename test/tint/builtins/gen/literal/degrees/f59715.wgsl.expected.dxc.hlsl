RWByteAddressBuffer prevent_dce : register(u0, space2);

void degrees_f59715() {
  vector<float16_t, 2> res = vector<float16_t, 2>(float16_t(57.3125h), float16_t(57.3125h));
  prevent_dce.Store<vector<float16_t, 2> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  degrees_f59715();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  degrees_f59715();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  degrees_f59715();
  return;
}
