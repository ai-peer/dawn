struct Inner {
  float3x2 m;
};
struct Outer {
  Inner a[4];
};

cbuffer cbuffer_a : register(b0, space0) {
  uint4 a[64];
};
static int counter = 0;

int i() {
  counter = (counter + 1);
  return counter;
}

float3x2 tint_symbol_12(uint4 buffer[64], uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  uint4 ubo_load = buffer[scalar_offset / 4];
  const uint scalar_offset_1 = ((offset + 8u)) / 4;
  uint4 ubo_load_1 = buffer[scalar_offset_1 / 4];
  const uint scalar_offset_2 = ((offset + 16u)) / 4;
  uint4 ubo_load_2 = buffer[scalar_offset_2 / 4];
  return float3x2(asfloat(((scalar_offset & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_1 & 2) ? ubo_load_1.zw : ubo_load_1.xy)), asfloat(((scalar_offset_2 & 2) ? ubo_load_2.zw : ubo_load_2.xy)));
}

Inner tint_symbol_11(uint4 buffer[64], uint offset) {
  const Inner tint_symbol_15 = {tint_symbol_12(buffer, (offset + 0u))};
  return tint_symbol_15;
}

typedef Inner tint_symbol_10_ret[4];
tint_symbol_10_ret tint_symbol_10(uint4 buffer[64], uint offset) {
  Inner arr[4] = (Inner[4])0;
  {
    [loop] for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = tint_symbol_11(buffer, (offset + (i_1 * 64u)));
    }
  }
  return arr;
}

Outer tint_symbol_9(uint4 buffer[64], uint offset) {
  const Outer tint_symbol_16 = {tint_symbol_10(buffer, (offset + 0u))};
  return tint_symbol_16;
}

typedef Outer tint_symbol_8_ret[4];
tint_symbol_8_ret tint_symbol_8(uint4 buffer[64], uint offset) {
  Outer arr_1[4] = (Outer[4])0;
  {
    [loop] for(uint i_2 = 0u; (i_2 < 4u); i_2 = (i_2 + 1u)) {
      arr_1[i_2] = tint_symbol_9(buffer, (offset + (i_2 * 256u)));
    }
  }
  return arr_1;
}

[numthreads(1, 1, 1)]
void f() {
  const int tint_symbol = i();
  const uint p_a_i_save = uint(tint_symbol);
  const uint p_a_i_a_save = uint(tint_symbol);
  const int tint_symbol_1 = i();
  const uint p_a_i_a_i_save = uint(tint_symbol);
  const uint p_a_i_a_i_save_1 = uint(tint_symbol_1);
  const uint p_a_i_a_i_m_save = uint(tint_symbol);
  const uint p_a_i_a_i_m_save_1 = uint(tint_symbol_1);
  const int tint_symbol_2 = i();
  const uint p_a_i_a_i_m_i_save = uint(tint_symbol);
  const uint p_a_i_a_i_m_i_save_1 = uint(tint_symbol_1);
  const uint p_a_i_a_i_m_i_save_2 = uint(tint_symbol_2);
  const Outer l_a[4] = tint_symbol_8(a, 0u);
  const Outer l_a_i = tint_symbol_9(a, (256u * uint(tint_symbol)));
  const Inner l_a_i_a[4] = tint_symbol_10(a, (256u * uint(tint_symbol)));
  const Inner l_a_i_a_i = tint_symbol_11(a, ((256u * uint(tint_symbol)) + (64u * uint(tint_symbol_1))));
  const float3x2 l_a_i_a_i_m = tint_symbol_12(a, ((256u * uint(tint_symbol)) + (64u * uint(tint_symbol_1))));
  const uint scalar_offset_3 = ((((256u * uint(tint_symbol)) + (64u * uint(tint_symbol_1))) + (8u * uint(tint_symbol_2)))) / 4;
  uint4 ubo_load_3 = a[scalar_offset_3 / 4];
  const float2 l_a_i_a_i_m_i = asfloat(((scalar_offset_3 & 2) ? ubo_load_3.zw : ubo_load_3.xy));
  const uint tint_symbol_3 = uint(tint_symbol);
  const uint tint_symbol_4 = uint(tint_symbol_1);
  const uint tint_symbol_5 = uint(tint_symbol_2);
  const int tint_symbol_6 = i();
  const uint tint_symbol_7 = uint(tint_symbol_6);
  const uint scalar_offset_4 = (((((256u * tint_symbol_3) + (64u * tint_symbol_4)) + (8u * tint_symbol_5)) + (4u * tint_symbol_7))) / 4;
  const float l_a_i_a_i_m_i_i = asfloat(a[scalar_offset_4 / 4][scalar_offset_4 % 4]);
  return;
}
