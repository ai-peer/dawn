#version 310 es

struct S {
  mat3 m;
};

layout(binding = 0, std430) buffer tint_symbol_block_ssbo {
  S inner;
} tint_symbol;

layout(binding = 1, std430) buffer buffer2_block_ssbo {
  S inner[4];
} buffer2;

void assign_and_preserve_padding_1_tint_symbol_m(mat3 value) {
  tint_symbol.inner.m[0] = value[0u];
  tint_symbol.inner.m[1] = value[1u];
  tint_symbol.inner.m[2] = value[2u];
}

void assign_and_preserve_padding_1_buffer2_X_m(uint dest[1], mat3 value) {
  buffer2.inner[dest[0]].m[0] = value[0u];
  buffer2.inner[dest[0]].m[1] = value[1u];
  buffer2.inner[dest[0]].m[2] = value[2u];
}

void assign_and_preserve_padding_tint_symbol(S value) {
  assign_and_preserve_padding_1_tint_symbol_m(value.m);
}

void assign_and_preserve_padding_buffer2_X(uint dest[1], S value) {
  uint tint_symbol_2[1] = uint[1](dest[0u]);
  assign_and_preserve_padding_1_buffer2_X_m(tint_symbol_2, value.m);
}

void assign_and_preserve_padding_2_buffer2(S value[4]) {
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      uint tint_symbol_3[1] = uint[1](i);
      assign_and_preserve_padding_buffer2_X(tint_symbol_3, value[i]);
    }
  }
}

void tint_symbol_1() {
  mat3 m = mat3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  {
    for(uint c = 0u; (c < 3u); c = (c + 1u)) {
      m[c] = vec3(float(((c * 3u) + 1u)), float(((c * 3u) + 2u)), float(((c * 3u) + 3u)));
    }
  }
  S tint_symbol_4 = S(m);
  assign_and_preserve_padding_tint_symbol(tint_symbol_4);
  S tint_symbol_5 = S(m);
  S tint_symbol_6 = S((m * 2.0f));
  S tint_symbol_7 = S((m * 3.0f));
  S tint_symbol_8 = S((m * 4.0f));
  S tint_symbol_9[4] = S[4](tint_symbol_5, tint_symbol_6, tint_symbol_7, tint_symbol_8);
  assign_and_preserve_padding_2_buffer2(tint_symbol_9);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol_1();
  return;
}
