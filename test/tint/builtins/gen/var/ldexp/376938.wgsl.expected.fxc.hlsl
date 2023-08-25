void ldexp_376938() {
  int4 arg_1 = int4(1, 1, 1, 1);
  float4 res = ldexp(float4(1.0f, 1.0f, 1.0f, 1.0f), arg_1);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  ldexp_376938();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  ldexp_376938();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  ldexp_376938();
  return;
}
