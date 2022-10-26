cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[16];
};

void a(float4x4 a_1[4]) {
}

void b(float4x4 m) {
}

void c(float4 v) {
}

void d(float f_1) {
}

float4x4 tint_symbol_1(uint4 buffer[16], uint offset) {
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

typedef float4x4 tint_symbol_ret[4];
tint_symbol_ret tint_symbol(uint4 buffer[16], uint offset) {
  float4x4 arr[4] = (float4x4[4])0;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      arr[i] = tint_symbol_1(buffer, (offset + (i * 64u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  a(tint_symbol(u, 0u));
  b(tint_symbol_1(u, 64u));
  c(asfloat(u[4]).ywxz);
  d(asfloat(u[4]).ywxz.x);
  return;
}
