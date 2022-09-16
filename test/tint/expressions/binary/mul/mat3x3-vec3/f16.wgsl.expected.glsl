#version 310 es
#extension GL_AMD_gpu_shader_half_float : require
precision mediump float;

layout(binding = 0, std140) uniform S_std140_ubo {
  f16vec3 matrix_0;
  f16vec3 matrix_1;
  f16vec3 matrix_2;
  f16vec3 vector;
} data;

f16mat3 load_data_matrix() {
  return f16mat3(data.matrix_0, data.matrix_1, data.matrix_2);
}

void tint_symbol() {
  f16vec3 x = (load_data_matrix() * data.vector);
}

void main() {
  tint_symbol();
  return;
}
