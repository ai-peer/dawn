struct tint_symbol_3 {
  float4 Position : SV_Position;
};
struct tint_symbol_2 {
  float4 color;
  float4 Position;
};

tint_symbol_3 truncate_shader_output(tint_symbol_2 io) {
  const tint_symbol_3 tint_symbol_4 = {io.Position};
  return tint_symbol_4;
}

struct Output {
  float4 Position;
  float4 color;
};
struct tint_symbol_1 {
  uint VertexIndex : SV_VertexID;
  uint InstanceIndex : SV_InstanceID;
};

Output main_inner(uint VertexIndex, uint InstanceIndex) {
  const float2 zv[4] = {(0.20000000298023223877f).xx, (0.30000001192092895508f).xx, (-0.10000000149011611938f).xx, (1.10000002384185791016f).xx};
  const float z = zv[InstanceIndex].x;
  Output output = (Output)0;
  output.Position = float4(0.5f, 0.5f, z, 1.0f);
  const float4 colors[4] = {float4(1.0f, 0.0f, 0.0f, 1.0f), float4(0.0f, 1.0f, 0.0f, 1.0f), float4(0.0f, 0.0f, 1.0f, 1.0f), (1.0f).xxxx};
  output.color = colors[InstanceIndex];
  return output;
}

tint_symbol_3 main(tint_symbol_1 tint_symbol) {
  const Output inner_result = main_inner(tint_symbol.VertexIndex, tint_symbol.InstanceIndex);
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.Position = inner_result.Position;
  wrapper_result.color = inner_result.color;
  return truncate_shader_output(wrapper_result);
}
