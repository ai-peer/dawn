cbuffer cbuffer_uniforms : register(b0, space0) {
  uint4 uniforms[4];
};

struct VertexInput {
  float4 cur_position;
  float4 color;
};
struct VertexOutput {
  float4 vtxFragColor;
  float4 Position;
};
struct tint_symbol_1 {
  float4 cur_position : TEXCOORD0;
  float4 color : TEXCOORD1;
};
struct tint_symbol_2 {
  float4 vtxFragColor : TEXCOORD0;
  float4 Position : SV_Position;
};

float4x4 tint_symbol_6(uint4 buffer[4], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  const uint scalar_offset_bytes_3 = ((offset + 48u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  return float4x4(asfloat(buffer[scalar_offset_index / 4]), asfloat(buffer[scalar_offset_index_1 / 4]), asfloat(buffer[scalar_offset_index_2 / 4]), asfloat(buffer[scalar_offset_index_3 / 4]));
}

VertexOutput vtx_main_inner(VertexInput input) {
  VertexOutput output = (VertexOutput)0;
  output.Position = mul(input.cur_position, tint_symbol_6(uniforms, 0u));
  output.vtxFragColor = input.color;
  return output;
}

tint_symbol_2 vtx_main(tint_symbol_1 tint_symbol) {
  const VertexInput tint_symbol_8 = {tint_symbol.cur_position, tint_symbol.color};
  const VertexOutput inner_result = vtx_main_inner(tint_symbol_8);
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.vtxFragColor = inner_result.vtxFragColor;
  wrapper_result.Position = inner_result.Position;
  return wrapper_result;
}

struct tint_symbol_4 {
  float4 fragColor : TEXCOORD0;
};
struct tint_symbol_5 {
  float4 value : SV_Target0;
};

float4 frag_main_inner(float4 fragColor) {
  return fragColor;
}

tint_symbol_5 frag_main(tint_symbol_4 tint_symbol_3) {
  const float4 inner_result_1 = frag_main_inner(tint_symbol_3.fragColor);
  tint_symbol_5 wrapper_result_1 = (tint_symbol_5)0;
  wrapper_result_1.value = inner_result_1;
  return wrapper_result_1;
}
