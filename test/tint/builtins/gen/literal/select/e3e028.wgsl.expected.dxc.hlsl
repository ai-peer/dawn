RWByteAddressBuffer prevent_dce : register(u0, space2);

void select_e3e028() {
  bool4 res = bool4(true, true, true, true);
  prevent_dce.Store(0u, asuint((all((res == bool4(false, false, false, false))) ? 1 : 0)));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  select_e3e028();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  select_e3e028();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  select_e3e028();
  return;
}
