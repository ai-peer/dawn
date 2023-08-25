vector<float16_t, 4> tint_trunc(vector<float16_t, 4> param_0) {
  return param_0 < 0 ? ceil(param_0) : floor(param_0);
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void trunc_ce7c17() {
  vector<float16_t, 4> arg_0 = vector<float16_t, 4>(float16_t(1.5h), float16_t(1.5h), float16_t(1.5h), float16_t(1.5h));
  vector<float16_t, 4> res = tint_trunc(arg_0);
  prevent_dce.Store<vector<float16_t, 4> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  trunc_ce7c17();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  trunc_ce7c17();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  trunc_ce7c17();
  return;
}
