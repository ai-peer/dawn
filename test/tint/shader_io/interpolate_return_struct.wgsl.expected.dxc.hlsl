struct tint_symbol_1 {
  float4 pos : SV_Position;
};
struct tint_symbol {
  float none;
  float flat;
  float perspective_center;
  float perspective_centroid;
  float perspective_sample;
  float linear_center;
  float linear_centroid;
  float linear_sample;
  float4 pos;
};

tint_symbol_1 truncate_shader_output(tint_symbol io) {
  const tint_symbol_1 tint_symbol_2 = {io.pos};
  return tint_symbol_2;
}

struct Out {
  float4 pos;
  float none;
  float flat;
  float perspective_center;
  float perspective_centroid;
  float perspective_sample;
  float linear_center;
  float linear_centroid;
  float linear_sample;
};

Out main_inner() {
  const Out tint_symbol_3 = (Out)0;
  return tint_symbol_3;
}

tint_symbol_1 main() {
  const Out inner_result = main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.pos = inner_result.pos;
  wrapper_result.none = inner_result.none;
  wrapper_result.flat = inner_result.flat;
  wrapper_result.perspective_center = inner_result.perspective_center;
  wrapper_result.perspective_centroid = inner_result.perspective_centroid;
  wrapper_result.perspective_sample = inner_result.perspective_sample;
  wrapper_result.linear_center = inner_result.linear_center;
  wrapper_result.linear_centroid = inner_result.linear_centroid;
  wrapper_result.linear_sample = inner_result.linear_sample;
  return truncate_shader_output(wrapper_result);
}
