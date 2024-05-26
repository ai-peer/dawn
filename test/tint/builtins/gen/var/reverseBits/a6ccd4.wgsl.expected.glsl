#version 310 es

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  uvec3 inner;
  uint pad;
} prevent_dce;

void reverseBits_a6ccd4() {
  uvec3 arg_0 = uvec3(1u);
  uvec3 res = bitfieldReverse(arg_0);
  prevent_dce.inner = res;
}

vec4 vertex_main() {
  reverseBits_a6ccd4();
  return vec4(0.0f);
}

void main() {
  gl_PointSize = 1.0;
  vec4 inner_result = vertex_main();
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  uvec3 inner;
  uint pad;
} prevent_dce;

void reverseBits_a6ccd4() {
  uvec3 arg_0 = uvec3(1u);
  uvec3 res = bitfieldReverse(arg_0);
  prevent_dce.inner = res;
}

void fragment_main() {
  reverseBits_a6ccd4();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  uvec3 inner;
  uint pad;
} prevent_dce;

void reverseBits_a6ccd4() {
  uvec3 arg_0 = uvec3(1u);
  uvec3 res = bitfieldReverse(arg_0);
  prevent_dce.inner = res;
}

void compute_main() {
  reverseBits_a6ccd4();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}