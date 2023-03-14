cbuffer cbuffer_tint_symbol_2 : register(b30, space0) {
  uint4 tint_symbol_2[1];
};

RWByteAddressBuffer sb_rw : register(u0, space0);

RWByteAddressBuffer prevent_dce : register(u0, space2);

void arrayLength_cbd6b5() {
  uint res = ((tint_symbol_2[0].x - 0u) / 2u);
  prevent_dce.Store(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  arrayLength_cbd6b5();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  arrayLength_cbd6b5();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  arrayLength_cbd6b5();
  return;
}
