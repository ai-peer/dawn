RWByteAddressBuffer prevent_dce : register(u0, space2);

void acos_004aff() {
  vector<float16_t, 2> res = vector<float16_t, 2>(float16_t(0.25048828125h), float16_t(0.25048828125h));
  prevent_dce.Store<vector<float16_t, 2> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  acos_004aff();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  acos_004aff();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  acos_004aff();
  return;
}
