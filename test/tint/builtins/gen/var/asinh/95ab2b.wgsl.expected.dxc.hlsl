vector<float16_t, 4> tint_sinh(vector<float16_t, 4> x) {
  return log((x + sqrt(((x * x) + float16_t(1.0h)))));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void asinh_95ab2b() {
  vector<float16_t, 4> arg_0 = vector<float16_t, 4>(float16_t(1.0h), float16_t(1.0h), float16_t(1.0h), float16_t(1.0h));
  vector<float16_t, 4> res = tint_sinh(arg_0);
  prevent_dce.Store<vector<float16_t, 4> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  asinh_95ab2b();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  asinh_95ab2b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  asinh_95ab2b();
  return;
}
