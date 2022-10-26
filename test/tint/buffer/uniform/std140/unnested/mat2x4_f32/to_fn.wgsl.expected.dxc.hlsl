cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[2];
};

void a(float2x4 m) {
}

void b(float4 v) {
}

void c(float f_1) {
}

float2x4 tint_symbol(uint4 buffer[2], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  return float2x4(asfloat(buffer[scalar_offset_index / 4]), asfloat(buffer[scalar_offset_index_1 / 4]));
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
