RWByteAddressBuffer prevent_dce : register(u0, space2);

void atanh_f3e01b() {
  float4 res = (0.54930615425109863281f).xxxx;
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  atanh_f3e01b();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  atanh_f3e01b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atanh_f3e01b();
  return;
}