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
  mat2x3 g;
  mat3x2 h;
  Inner i;
  Inner j[4];
};

layout(binding = 0, std430) buffer S_1 {
  S _;
} s;
void tint_symbol() {
  ivec3 a = s._.a;
  int b = s._.b;
  uvec3 c = s._.c;
  uint d = s._.d;
  vec3 e = s._.e;
  float f = s._.f;
  mat2x3 g = s._.g;
  mat3x2 h = s._.h;
  Inner i = s._.i;
  Inner j[4] = s._.j;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
