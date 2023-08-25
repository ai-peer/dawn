RWByteAddressBuffer prevent_dce : register(u0, space2);

void reflect_f47fdb() {
  float3 arg_0 = float3(1.0f, 1.0f, 1.0f);
  float3 arg_1 = float3(1.0f, 1.0f, 1.0f);
  float3 res = reflect(arg_0, arg_1);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  reflect_f47fdb();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  reflect_f47fdb();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  reflect_f47fdb();
  return;
}
