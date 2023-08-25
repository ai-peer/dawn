RWByteAddressBuffer prevent_dce : register(u0, space2);

void sign_160933() {
  vector<float16_t, 4> res = vector<float16_t, 4>(float16_t(1.0h), float16_t(1.0h), float16_t(1.0h), float16_t(1.0h));
  prevent_dce.Store<vector<float16_t, 4> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  sign_160933();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  sign_160933();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  sign_160933();
  return;
}
