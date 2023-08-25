struct Inner {
  int scalar_i32;
  float scalar_f32;
};

RWByteAddressBuffer sb : register(u0);

void sb_store_12(uint offset, float2x2 value) {
  sb.Store2((offset + 0u), asuint(value[0u]));
  sb.Store2((offset + 8u), asuint(value[1u]));
}

void sb_store_13(uint offset, float2x3 value) {
  sb.Store3((offset + 0u), asuint(value[0u]));
  sb.Store3((offset + 16u), asuint(value[1u]));
}

void sb_store_14(uint offset, float2x4 value) {
  sb.Store4((offset + 0u), asuint(value[0u]));
  sb.Store4((offset + 16u), asuint(value[1u]));
}

void sb_store_15(uint offset, float3x2 value) {
  sb.Store2((offset + 0u), asuint(value[0u]));
  sb.Store2((offset + 8u), asuint(value[1u]));
  sb.Store2((offset + 16u), asuint(value[2u]));
}

void sb_store_16(uint offset, float3x3 value) {
  sb.Store3((offset + 0u), asuint(value[0u]));
  sb.Store3((offset + 16u), asuint(value[1u]));
  sb.Store3((offset + 32u), asuint(value[2u]));
}

void sb_store_17(uint offset, float3x4 value) {
  sb.Store4((offset + 0u), asuint(value[0u]));
  sb.Store4((offset + 16u), asuint(value[1u]));
  sb.Store4((offset + 32u), asuint(value[2u]));
}

void sb_store_18(uint offset, float4x2 value) {
  sb.Store2((offset + 0u), asuint(value[0u]));
  sb.Store2((offset + 8u), asuint(value[1u]));
  sb.Store2((offset + 16u), asuint(value[2u]));
  sb.Store2((offset + 24u), asuint(value[3u]));
}

void sb_store_19(uint offset, float4x3 value) {
  sb.Store3((offset + 0u), asuint(value[0u]));
  sb.Store3((offset + 16u), asuint(value[1u]));
  sb.Store3((offset + 32u), asuint(value[2u]));
  sb.Store3((offset + 48u), asuint(value[3u]));
}

void sb_store_20(uint offset, float4x4 value) {
  sb.Store4((offset + 0u), asuint(value[0u]));
  sb.Store4((offset + 16u), asuint(value[1u]));
  sb.Store4((offset + 32u), asuint(value[2u]));
  sb.Store4((offset + 48u), asuint(value[3u]));
}

void sb_store_21(uint offset, float3 value[2]) {
  float3 array_1[2] = value;
  {
    for(uint i = 0u; (i < 2u); i = (i + 1u)) {
      sb.Store3((offset + (i * 16u)), asuint(array_1[i]));
    }
  }
}

void sb_store_22(uint offset, Inner value) {
  sb.Store((offset + 0u), asuint(value.scalar_i32));
  sb.Store((offset + 4u), asuint(value.scalar_f32));
}

void sb_store_23(uint offset, Inner value[4]) {
  Inner array_2[4] = value;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      sb_store_22((offset + (i_1 * 8u)), array_2[i_1]);
    }
  }
}

[numthreads(1, 1, 1)]
void main() {
  sb.Store(0u, asuint(0.0f));
  sb.Store(4u, asuint(0));
  sb.Store(8u, asuint(0u));
  sb.Store2(16u, asuint(float2(0.0f, 0.0f)));
  sb.Store2(24u, asuint(int2(0, 0)));
  sb.Store2(32u, asuint(uint2(0u, 0u)));
  sb.Store3(48u, asuint(float3(0.0f, 0.0f, 0.0f)));
  sb.Store3(64u, asuint(int3(0, 0, 0)));
  sb.Store3(80u, asuint(uint3(0u, 0u, 0u)));
  sb.Store4(96u, asuint(float4(0.0f, 0.0f, 0.0f, 0.0f)));
  sb.Store4(112u, asuint(int4(0, 0, 0, 0)));
  sb.Store4(128u, asuint(uint4(0u, 0u, 0u, 0u)));
  sb_store_12(144u, float2x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f)));
  sb_store_13(160u, float2x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f)));
  sb_store_14(192u, float2x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f)));
  sb_store_15(224u, float3x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)));
  sb_store_16(256u, float3x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f)));
  sb_store_17(304u, float3x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f)));
  sb_store_18(352u, float4x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)));
  sb_store_19(384u, float4x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f)));
  sb_store_20(448u, float4x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f)));
  const float3 tint_symbol[2] = (float3[2])0;
  sb_store_21(512u, tint_symbol);
  const Inner tint_symbol_1 = (Inner)0;
  sb_store_22(544u, tint_symbol_1);
  const Inner tint_symbol_2[4] = (Inner[4])0;
  sb_store_23(552u, tint_symbol_2);
  return;
}
