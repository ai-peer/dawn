cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[4];
};

void a(float4x4 m) {
}

void b(float4 v) {
}

void c(float f_1) {
}

float4x4 tint_symbol(uint4 buffer[4], uint offset) {
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

[numthreads(1, 1, 1)]
void f() {
  a(tint_symbol(u, 0u));
  b(asfloat(u[1]));
  b(asfloat(u[1]).ywxz);
  c(asfloat(u[1].x));
  c(asfloat(u[1]).ywxz.x);
  return;
}
