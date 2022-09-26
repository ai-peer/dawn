#version 310 es

struct InnerS {
  int v;
};

struct S1 {
  InnerS a2[8];
};

layout(binding = 4, std140) uniform Uniforms_ubo {
  uint i;
  uint j;
  uint pad;
  uint pad_1;
} uniforms;

layout(binding = 0, std430) buffer OuterS_ssbo {
  S1 a1[];
} s;

void tint_symbol() {
  InnerS v = InnerS(0);
  uint tint_symbol_1 = uniforms.i;
  uint tint_symbol_2 = uniforms.j;
  s.a1[tint_symbol_1].a2[tint_symbol_2] = v;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
