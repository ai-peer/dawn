RWByteAddressBuffer prevent_dce : register(u0, space2);

void min_82b28f() {
  uint2 arg_0 = uint2(1u, 1u);
  uint2 arg_1 = uint2(1u, 1u);
  uint2 res = min(arg_0, arg_1);
  prevent_dce.Store2(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  min_82b28f();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  min_82b28f();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  min_82b28f();
  return;
}
