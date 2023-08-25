RWTexture2DArray<float4> arg_0 : register(u0, space1);

void textureStore_59a0ab() {
  uint2 arg_1 = uint2(1u, 1u);
  int arg_2 = 1;
  float4 arg_3 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  arg_0[uint3(arg_1, uint(arg_2))] = arg_3;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureStore_59a0ab();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureStore_59a0ab();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_59a0ab();
  return;
}
