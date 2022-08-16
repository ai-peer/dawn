#version 310 es

struct Inner {
  ivec3 a;
  int b;
  uvec3 c;
  uint d;
  vec3 e;
  float f;
  ivec2 g;
  ivec2 h;
  mat2x3 i;
  mat3x2 j;
  ivec4 k[4];
};

struct S {
  Inner arr[8];
};

layout(binding = 0) uniform S_1 {
  S _;
} s;

void tint_symbol(uint idx) {
  ivec3 a = s._.arr[idx].a;
  int b = s._.arr[idx].b;
  uvec3 c = s._.arr[idx].c;
  uint d = s._.arr[idx].d;
  vec3 e = s._.arr[idx].e;
  float f = s._.arr[idx].f;
  ivec2 g = s._.arr[idx].g;
  ivec2 h = s._.arr[idx].h;
  mat2x3 i = s._.arr[idx].i;
  mat3x2 j = s._.arr[idx].j;
  ivec4 k[4] = s._.arr[idx].k;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol(gl_LocalInvocationIndex);
  return;
}
