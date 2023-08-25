RWByteAddressBuffer prevent_dce : register(u0, space2);

void ldexp_217a31() {
  vector<float16_t, 2> arg_0 = vector<float16_t, 2>(float16_t(1.0h), float16_t(1.0h));
  vector<float16_t, 2> res = ldexp(arg_0, int2(1, 1));
  prevent_dce.Store<vector<float16_t, 2> >(0u, res);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  ldexp_217a31();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  ldexp_217a31();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  ldexp_217a31();
  return;
}
