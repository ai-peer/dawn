#version 310 es

struct Inner {
  int x;
};

struct S {
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
  Inner k;
  Inner l[4];
};

layout(binding = 0) uniform S_1 {
  S _;
} s;

void tint_symbol() {
  ivec3 a = s._.a;
  int b = s._.b;
  uvec3 c = s._.c;
  uint d = s._.d;
  vec3 e = s._.e;
  float f = s._.f;
  ivec2 g = s._.g;
  ivec2 h = s._.h;
  mat2x3 i = s._.i;
  mat3x2 j = s._.j;
  Inner k = s._.k;
  Inner l[4] = s._.l;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
