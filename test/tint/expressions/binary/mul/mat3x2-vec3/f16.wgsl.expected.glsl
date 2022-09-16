#version 310 es
#extension GL_AMD_gpu_shader_half_float : require
precision mediump float;

layout(binding = 0, std140) uniform S_std140_ubo {
  f16vec2 matrix_0;
  f16vec2 matrix_1;
  f16vec2 matrix_2;
  uint pad;
  f16vec3 vector;
  uint pad_1;
  uint pad_2;
} data;

f16mat3x2 load_data_matrix() {
  return f16mat3x2(data.matrix_0, data.matrix_1, data.matrix_2);
}

void tint_symbol() {
  f16vec2 x = (load_data_matrix() * data.vector);
}

void main() {
  tint_symbol();
  return;
}
