RWByteAddressBuffer prevent_dce : register(u0, space2);

void select_a081f1() {
  vector<float16_t, 4> arg_0 = vector<float16_t, 4>(float16_t(1.0h), float16_t(1.0h), float16_t(1.0h), float16_t(1.0h));
  vector<float16_t, 4> arg_1 = vector<float16_t, 4>(float16_t(1.0h), float16_t(1.0h), float16_t(1.0h), float16_t(1.0h));
  bool4 arg_2 = bool4(true, true, true, true);
  vector<float16_t, 4> res = (arg_2 ? arg_1 : arg_0);
  prevent_dce.Store<vector<float16_t, 4> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  select_a081f1();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  select_a081f1();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  select_a081f1();
  return;
}
