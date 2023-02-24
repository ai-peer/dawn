cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[8];
};

void a(float2x3 a_1[4]) {
}

void b(float2x3 m) {
}

void c(float3 v) {
}

void d(float f_1) {
}

float2x3 tint_symbol_1(uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  const uint scalar_offset_1 = ((offset + 16u)) / 4;
  return float2x3(asfloat(u[scalar_offset / 4].xyz), asfloat(u[scalar_offset_1 / 4].xyz));
}

typedef float2x3 tint_symbol_ret[4];
tint_symbol_ret tint_symbol(uint offset) {
  float2x3 arr[4] = (float2x3[4])0;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      arr[i] = tint_symbol_1((offset + (i * 32u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  a(tint_symbol(0u));
  b(tint_symbol_1(32u));
  c(asfloat(u[2].xyz).zxy);
  d(asfloat(u[2].xyz).zxy.x);
  return;
}
