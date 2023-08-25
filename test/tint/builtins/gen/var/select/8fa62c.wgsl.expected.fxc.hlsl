RWByteAddressBuffer prevent_dce : register(u0, space2);

void select_8fa62c() {
  int3 arg_0 = int3(1, 1, 1);
  int3 arg_1 = int3(1, 1, 1);
  bool arg_2 = true;
  int3 res = (arg_2 ? arg_1 : arg_0);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  select_8fa62c();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  select_8fa62c();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  select_8fa62c();
  return;
}
