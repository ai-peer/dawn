RWByteAddressBuffer prevent_dce : register(u0, space2);

void radians_09b7fc() {
  float4 res = float4(0.01745329238474369049f, 0.01745329238474369049f, 0.01745329238474369049f, 0.01745329238474369049f);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  radians_09b7fc();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  radians_09b7fc();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  radians_09b7fc();
  return;
}
