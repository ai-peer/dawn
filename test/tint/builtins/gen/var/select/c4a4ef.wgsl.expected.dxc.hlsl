RWByteAddressBuffer prevent_dce : register(u0, space2);

void select_c4a4ef() {
  uint4 arg_0 = uint4(1u, 1u, 1u, 1u);
  uint4 arg_1 = uint4(1u, 1u, 1u, 1u);
  bool4 arg_2 = bool4(true, true, true, true);
  uint4 res = (arg_2 ? arg_1 : arg_0);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  select_c4a4ef();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  select_c4a4ef();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  select_c4a4ef();
  return;
}
