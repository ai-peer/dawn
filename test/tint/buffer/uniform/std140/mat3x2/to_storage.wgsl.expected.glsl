SKIP: FAILED

#version 310 es

struct S {
  int before;
  uint pad;
  mat3x2 m;
  uint pad_1;
  uint pad_2;
  uint pad_3;
  uint pad_4;
  uint pad_5;
  uint pad_6;
  uint pad_7;
  uint pad_8;
  uint pad_9;
  uint pad_10;
  int after;
  uint pad_11;
};

struct S_std140 {
  int before;
  uint pad_12;
  vec2 m_0;
  vec2 m_1;
  vec2 m_2;
  uint pad_13;
  uint pad_14;
  uint pad_15;
  uint pad_16;
  uint pad_17;
  uint pad_18;
  uint pad_19;
  uint pad_20;
  uint pad_21;
  uint pad_22;
  int after;
  uint pad_23;
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
  S tint_symbol = S(val.before, mat3x2(val.m_0, val.m_1, val.m_2), val.after);
  return tint_symbol;
}

S[4] conv_arr_4_S(S_std140 val[4]) {
  S arr[4] = S[4](S(0, mat3x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0), S(0, mat3x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0), S(0, mat3x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0), S(0, mat3x2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f), 0));
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      arr[i] = conv_S(val[i]);
    }
  }
  return arr;
}

mat3x2 load_u_2_m() {
  return mat3x2(u.inner[2u].m_0, u.inner[2u].m_1, u.inner[2u].m_2);
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
ERROR: 0:57: 'constructor' : Number of constructor parameters does not match the number of structure fields
ERROR: 0:57: '=' :  cannot convert from ' const float' to ' temp structure{ global highp int before,  global highp uint pad,  global highp 3X2 matrix of float m,  global highp uint pad_1,  global highp uint pad_2,  global highp uint pad_3,  global highp uint pad_4,  global highp uint pad_5,  global highp uint pad_6,  global highp uint pad_7,  global highp uint pad_8,  global highp uint pad_9,  global highp uint pad_10,  global highp int after,  global highp uint pad_11}'
ERROR: 0:57: '' : compilation terminated
ERROR: 3 compilation errors.  No code generated.



