vector<float16_t, 4> tint_degrees(vector<float16_t, 4> param_0) {
  return param_0 * 57.29577951308232286465;
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void degrees_3055d3() {
  vector<float16_t, 4> arg_0 = vector<float16_t, 4>(float16_t(1.0h), float16_t(1.0h), float16_t(1.0h), float16_t(1.0h));
  vector<float16_t, 4> res = tint_degrees(arg_0);
  prevent_dce.Store<vector<float16_t, 4> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  degrees_3055d3();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  degrees_3055d3();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  degrees_3055d3();
  return;
}
