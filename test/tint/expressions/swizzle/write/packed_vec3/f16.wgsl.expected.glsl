#version 310 es
#extension GL_AMD_gpu_shader_half_float : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
layout(binding = 0, std430) buffer S_ssbo {
  f16vec3 v;
} U;

void f() {
  U.v = f16vec3(1.0hf, 2.0hf, 3.0hf);
  U.v.x = 1.0hf;
  U.v.y = 2.0hf;
  U.v.z = 3.0hf;
}

