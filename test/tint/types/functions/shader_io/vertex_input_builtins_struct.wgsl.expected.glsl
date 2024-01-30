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
struct VertexInputs {
  uint vertex_index;
  uint instance_index;
};

vec4 tint_symbol(VertexInputs inputs) {
  uint foo = (inputs.vertex_index + inputs.instance_index);
  return vec4(0.0f);
}

void main() {
  gl_PointSize = 1.0;
  VertexInputs tint_symbol_1 = VertexInputs(uint(gl_VertexID), (uint(gl_InstanceID) + push_constants.first_instance));
  vec4 inner_result = tint_symbol(tint_symbol_1);
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
