RWTexture2D<uint4> arg_0 : register(u0, space1);

void textureStore_e8cbf7() {
  uint2 arg_1 = uint2(1u, 1u);
  uint4 arg_2 = uint4(1u, 1u, 1u, 1u);
  arg_0[arg_1] = arg_2;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureStore_e8cbf7();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureStore_e8cbf7();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_e8cbf7();
  return;
}
