RWByteAddressBuffer sb : register(u0);

struct tint_symbol_1 {
  uint idx : SV_GroupIndex;
};

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

void main_inner(uint idx) {
  sb.Store((544u * idx), asuint(0.0f));
  sb.Store(((544u * idx) + 4u), asuint(0));
  sb.Store(((544u * idx) + 8u), asuint(0u));
  sb.Store2(((544u * idx) + 16u), asuint(float2(0.0f, 0.0f)));
  sb.Store2(((544u * idx) + 24u), asuint(int2(0, 0)));
  sb.Store2(((544u * idx) + 32u), asuint(uint2(0u, 0u)));
  sb.Store3(((544u * idx) + 48u), asuint(float3(0.0f, 0.0f, 0.0f)));
  sb.Store3(((544u * idx) + 64u), asuint(int3(0, 0, 0)));
  sb.Store3(((544u * idx) + 80u), asuint(uint3(0u, 0u, 0u)));
  sb.Store4(((544u * idx) + 96u), asuint(float4(0.0f, 0.0f, 0.0f, 0.0f)));
  sb.Store4(((544u * idx) + 112u), asuint(int4(0, 0, 0, 0)));
  sb.Store4(((544u * idx) + 128u), asuint(uint4(0u, 0u, 0u, 0u)));
  sb_store_12(((544u * idx) + 144u), float2x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f)));
  sb_store_13(((544u * idx) + 160u), float2x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f)));
  sb_store_14(((544u * idx) + 192u), float2x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f)));
  sb_store_15(((544u * idx) + 224u), float3x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)));
  sb_store_16(((544u * idx) + 256u), float3x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f)));
  sb_store_17(((544u * idx) + 304u), float3x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f)));
  sb_store_18(((544u * idx) + 352u), float4x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f)));
  sb_store_19(((544u * idx) + 384u), float4x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f)));
  sb_store_20(((544u * idx) + 448u), float4x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f)));
  const float3 tint_symbol_2[2] = (float3[2])0;
  sb_store_21(((544u * idx) + 512u), tint_symbol_2);
}

[numthreads(1, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.idx);
  return;
}
