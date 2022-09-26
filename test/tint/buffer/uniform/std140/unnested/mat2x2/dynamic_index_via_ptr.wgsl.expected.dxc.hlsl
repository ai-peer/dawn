cbuffer cbuffer_m : register(b0, space0) {
  uint4 m[1];
};
static int counter = 0;

int i() {
  counter = (counter + 1);
  return counter;
}

float2x2 tint_symbol_1(uint4 buffer[1], uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  uint4 ubo_load = buffer[scalar_offset / 4];
  const uint scalar_offset_1 = ((offset + 8u)) / 4;
  uint4 ubo_load_1 = buffer[scalar_offset_1 / 4];
  return float2x2(asfloat(((scalar_offset & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_1 & 2) ? ubo_load_1.zw : ubo_load_1.xy)));
}

[numthreads(1, 1, 1)]
void f() {
  const int tint_symbol = i();
  const uint p_m_i_save = uint(tint_symbol);
  const float2x2 l_m = tint_symbol_1(m, 0u);
  const uint scalar_offset_2 = ((8u * uint(tint_symbol))) / 4;
  uint4 ubo_load_2 = m[scalar_offset_2 / 4];
  const float2 l_m_i = asfloat(((scalar_offset_2 & 2) ? ubo_load_2.zw : ubo_load_2.xy));
  return;
}
