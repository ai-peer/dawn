RWByteAddressBuffer prevent_dce : register(u0, space2);

void fract_943cb1() {
  float2 arg_0 = float2(1.25f, 1.25f);
  float2 res = frac(arg_0);
  prevent_dce.Store2(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  fract_943cb1();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  fract_943cb1();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  fract_943cb1();
  return;
}
