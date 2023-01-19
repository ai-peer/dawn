struct vec4f_1 {
  int i;
};
struct tint_symbol_1 {
  uint VertexIndex : SV_VertexID;
};
struct tint_symbol_2 {
  float4 value : SV_Position;
};

float4 main_inner(uint VertexIndex) {
  const vec4f_1 s = (vec4f_1)0;
  const float f = 0.0f;
  const bool b = false;
  return (b ? (1.0f).xxxx : (0.0f).xxxx);
}

tint_symbol_2 main(tint_symbol_1 tint_symbol) {
  const float4 inner_result = main_inner(tint_symbol.VertexIndex);
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}
