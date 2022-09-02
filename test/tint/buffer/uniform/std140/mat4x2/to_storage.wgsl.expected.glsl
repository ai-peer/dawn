SKIP: FAILED

#version 310 es

struct S {
  int before;
  uint pad;
  mat4x2 m;
  int after;
  uint pad_1;
};

struct S_std140 {
  int before;
  uint pad_2;
  vec2 m_0;
  vec2 m_1;
  vec2 m_2;
  vec2 m_3;
  int after;
  uint pad_3;
};

struct u_block {
  S_std140 inner[4];
};

layout(binding = 0, std140) uniform u_block_1 {
  S_std140 inner[4];
} u;

struct s_block {
  S inner[4];
};

layout(binding = 1, std430) buffer s_block_1 {
  S inner[4];
} s;
S conv_S(S_std140 val) {
  S tint_symbol = S(val.before, mat4x2(val.m_0, val.m_1, val.m_2, val.m_3), val.after);
  return tint_symbol;
}

S[4] conv_arr_4_S(S_std140 val[4]) {
  S arr[4] = S[4](S(0, mat4x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0), S(0, mat4x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0), S(0, mat4x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0), S(0, mat4x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0));
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      arr[i] = conv_S(val[i]);
    }
  }
  return arr;
}

mat4x2 load_u_2_m() {
  return mat4x2(u.inner[2u].m_0, u.inner[2u].m_1, u.inner[2u].m_2, u.inner[2u].m_3);
}

void f() {
  s.inner = conv_arr_4_S(u.inner);
  s.inner[1] = conv_S(u.inner[2u]);
  s.inner[3].m = load_u_2_m();
  s.inner[1].m[0] = u.inner[0u].m_1.yx;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
Error parsing GLSL shader:
ERROR: 0:38: 'constructor' : Number of constructor parameters does not match the number of structure fields
ERROR: 0:38: '=' :  cannot convert from ' const float' to ' temp structure{ global highp int before,  global highp uint pad,  global highp 4X2 matrix of float m,  global highp int after,  global highp uint pad_1}'
ERROR: 0:38: '' : compilation terminated
ERROR: 3 compilation errors.  No code generated.



