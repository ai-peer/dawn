#version 310 es

int tint_ftoi(float v) {
  return ((v < 2147483520.0f) ? ((v < -2147483648.0f) ? (-2147483647 - 1) : int(v)) : 2147483647);
}

layout(binding = 0, std430) buffer s_block_ssbo {
  int inner;
} s;

shared int g1;
int d(int val) {
  return val;
}

int c(inout int val) {
  int tint_symbol_1 = val;
  int tint_symbol_2 = d(val);
  return (tint_symbol_1 + tint_symbol_2);
}

int a(inout int val) {
  int tint_symbol_3 = val;
  int tint_symbol_4 = c(val);
  return (tint_symbol_3 + tint_symbol_4);
}

int z() {
  return atomicOr(g1, 0);
}

int y(inout vec3 v1) {
  v1.x = cross(v1, v1).x;
  return tint_ftoi(v1.x);
}

struct S {
  int a;
  int b;
};

int b(inout S val) {
  return (val.a + val.b);
}

void tint_symbol(uint local_invocation_index) {
  {
    atomicExchange(g1, 0);
  }
  barrier();
  int v1 = 0;
  S v2 = S(0, 0);
  vec3 v4 = vec3(0.0f);
  int t1 = atomicOr(g1, 0);
  int tint_symbol_5 = a(v1);
  int tint_symbol_6 = b(v2);
  int tint_symbol_7 = b(v2);
  int tint_symbol_8 = z();
  int tint_symbol_9 = t1;
  int tint_symbol_10 = y(v4);
  s.inner = (((((tint_symbol_5 + tint_symbol_6) + tint_symbol_7) + tint_symbol_8) + tint_symbol_9) + tint_symbol_10);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol(gl_LocalInvocationIndex);
  return;
}
