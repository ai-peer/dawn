struct tint_symbol_3 {
  float4 pos : SV_Position;
};
struct tint_symbol {
  float col1;
  float16_t col2;
  float4 pos;
};

tint_symbol_3 truncate_shader_output(tint_symbol io) {
  const tint_symbol_3 tint_symbol_4 = {io.pos};
  return tint_symbol_4;
}

struct Interface {
  float col1;
  float16_t col2;
  float4 pos;
};

Interface vert_main_inner() {
  const Interface tint_symbol_5 = {0.40000000596046447754f, float16_t(0.599609375h), (0.0f).xxxx};
  return tint_symbol_5;
}

tint_symbol_3 vert_main() {
  const Interface inner_result = vert_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.col1 = inner_result.col1;
  wrapper_result.col2 = inner_result.col2;
  wrapper_result.pos = inner_result.pos;
  return truncate_shader_output(wrapper_result);
}

struct tint_symbol_2 {
  float col1 : TEXCOORD1;
  float16_t col2 : TEXCOORD2;
  float4 pos : SV_Position;
};

void frag_main_inner(Interface colors) {
  const float r = colors.col1;
  const float16_t g = colors.col2;
}

void frag_main(tint_symbol_2 tint_symbol_1) {
  const Interface tint_symbol_6 = {tint_symbol_1.col1, tint_symbol_1.col2, tint_symbol_1.pos};
  frag_main_inner(tint_symbol_6);
  return;
}
