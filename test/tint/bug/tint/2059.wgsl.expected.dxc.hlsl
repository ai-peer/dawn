void set_vector_float3x3(inout float3x3 mat, int col, float3 val) {
  switch (col) {
    case 0: mat[0] = val; break;
    case 1: mat[1] = val; break;
    case 2: mat[2] = val; break;
  }
}

struct S {
  float3x3 m;
};

RWByteAddressBuffer buffer : register(u0);
RWByteAddressBuffer buffer2 : register(u1);

void buffer_store_1(uint offset, float3x3 value) {
  buffer.Store3((offset + 0u), asuint(value[0u]));
  buffer.Store3((offset + 16u), asuint(value[1u]));
  buffer.Store3((offset + 32u), asuint(value[2u]));
}

void buffer_store(uint offset, S value) {
  buffer_store_1((offset + 0u), value.m);
}

void buffer2_store_2(uint offset, float3x3 value) {
  buffer2.Store3((offset + 0u), asuint(value[0u]));
  buffer2.Store3((offset + 16u), asuint(value[1u]));
  buffer2.Store3((offset + 32u), asuint(value[2u]));
}

void buffer2_store_1(uint offset, S value) {
  buffer2_store_2((offset + 0u), value.m);
}

void buffer2_store(uint offset, S value[4]) {
  S array_1[4] = value;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      buffer2_store_1((offset + (i * 48u)), array_1[i]);
    }
  }
}

[numthreads(1, 1, 1)]
void main() {
  float3x3 m = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  {
    for(uint c = 0u; (c < 3u); c = (c + 1u)) {
      set_vector_float3x3(m, c, float3(float(((c * 3u) + 1u)), float(((c * 3u) + 2u)), float(((c * 3u) + 3u))));
    }
  }
  S tint_symbol = {m};
  buffer_store(0u, tint_symbol);
  S tint_symbol_1 = {m};
  S tint_symbol_2 = {(m * 2.0f)};
  S tint_symbol_3 = {(m * 3.0f)};
  S tint_symbol_4 = {(m * 4.0f)};
  const S tint_symbol_5[4] = {tint_symbol_1, tint_symbol_2, tint_symbol_3, tint_symbol_4};
  buffer2_store(0u, tint_symbol_5);
  return;
}
