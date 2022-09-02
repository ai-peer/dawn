#version 310 es
precision mediump float;

struct S {
  mat4x3 matrix;
  vec3 vector;
  uint pad;
};

layout(binding = 0, std140) uniform S_1 {
  mat4x3 matrix;
  vec3 vector;
  uint pad_1;
} data;

void tint_symbol() {
  vec4 x = (data.vector * data.matrix);
}

void main() {
  tint_symbol();
  return;
}
