RWByteAddressBuffer prevent_dce : register(u0, space2);

void sign_926015() {
  int2 arg_0 = int2(1, 1);
  int2 res = int2(sign(arg_0));
  prevent_dce.Store2(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  sign_926015();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  sign_926015();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  sign_926015();
  return;
}
