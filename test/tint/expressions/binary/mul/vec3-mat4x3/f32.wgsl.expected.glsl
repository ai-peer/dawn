#version 310 es
precision mediump float;

struct S {
  mat4x3 matrix;
  vec3 vector;
};

layout(binding = 0) uniform S_1 {
  S _;
} data;

void tint_symbol() {
  vec4 x = (data._.vector * data._.matrix);
}

void main() {
  tint_symbol();
  return;
}
