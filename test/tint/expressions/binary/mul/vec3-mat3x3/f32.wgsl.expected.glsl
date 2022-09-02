#version 310 es
precision mediump float;

struct S {
  mat3 matrix;
  vec3 vector;
  uint pad;
};

layout(binding = 0, std140) uniform S_1 {
  mat3 matrix;
  vec3 vector;
  uint pad_1;
} data;

void tint_symbol() {
  vec3 x = (data.vector * data.matrix);
}

void main() {
  tint_symbol();
  return;
}
