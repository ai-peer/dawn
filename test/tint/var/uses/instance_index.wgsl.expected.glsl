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
vec4 tint_symbol(uint b) {
  return vec4(float(b));
}

void main() {
  gl_PointSize = 1.0;
  vec4 inner_result = tint_symbol((uint(gl_InstanceID) + push_constants.first_instance));
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
