RWByteAddressBuffer prevent_dce : register(u0, space2);

void ldexp_cc9cde() {
  float4 arg_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  int4 arg_1 = int4(1, 1, 1, 1);
  float4 res = ldexp(arg_0, arg_1);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  ldexp_cc9cde();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  ldexp_cc9cde();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  ldexp_cc9cde();
  return;
}
