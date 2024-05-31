#version 310 es

layout(rgba16ui) uniform highp writeonly uimage3D arg_0;
void textureStore_1efc36() {
  uvec3 arg_1 = uvec3(1u);
  uvec4 arg_2 = uvec4(1u);
  imageStore(arg_0, ivec3(arg_1), arg_2);
}

vec4 vertex_main() {
  textureStore_1efc36();
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

layout(rgba16ui) uniform highp writeonly uimage3D arg_0;
void textureStore_1efc36() {
  uvec3 arg_1 = uvec3(1u);
  uvec4 arg_2 = uvec4(1u);
  imageStore(arg_0, ivec3(arg_1), arg_2);
}

void fragment_main() {
  textureStore_1efc36();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(rgba16ui) uniform highp writeonly uimage3D arg_0;
void textureStore_1efc36() {
  uvec3 arg_1 = uvec3(1u);
  uvec4 arg_2 = uvec4(1u);
  imageStore(arg_0, ivec3(arg_1), arg_2);
}

void compute_main() {
  textureStore_1efc36();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}