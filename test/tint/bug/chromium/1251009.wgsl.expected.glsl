#version 310 es

struct PushConstants {
  uint pad;
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
  uint pad_11;
  uint first_instance;
};

layout(location=0) uniform PushConstants push_constants;
layout(location = 0) in int loc0_1;
layout(location = 1) in uint loc1_1;
layout(location = 2) in uint loc1_2;
layout(location = 3) in vec4 loc3_1;
struct VertexInputs0 {
  uint vertex_index;
  int loc0;
};

struct VertexInputs1 {
  uint loc1;
  vec4 loc3;
};

vec4 tint_symbol(VertexInputs0 inputs0, uint loc1, uint instance_index, VertexInputs1 inputs1) {
  uint foo = (inputs0.vertex_index + instance_index);
  return vec4(0.0f);
}

void main() {
  gl_PointSize = 1.0;
  VertexInputs0 tint_symbol_1 = VertexInputs0(uint(gl_VertexID), loc0_1);
  VertexInputs1 tint_symbol_2 = VertexInputs1(loc1_2, loc3_1);
  vec4 inner_result = tint_symbol(tint_symbol_1, loc1_1, (uint(gl_InstanceID) + push_constants.first_instance), tint_symbol_2);
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
