void select_4c4738() {
  bool4 arg_2 = bool4(true, true, true, true);
  int4 res = (arg_2 ? int4(1, 1, 1, 1) : int4(1, 1, 1, 1));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  select_4c4738();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  select_4c4738();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  select_4c4738();
  return;
}
