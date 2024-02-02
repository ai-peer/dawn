#version 310 es

struct PushConstants {
  uint first_instance;
};

struct push_constants_block {
  PushConstants inner;
};

layout(location=0) uniform push_constants_block push_constants;
vec4 tint_symbol(uint b) {
  return vec4(float((b + push_constants.inner.first_instance)));
}

void main() {
  gl_PointSize = 1.0;
  vec4 inner_result = tint_symbol(uint(gl_InstanceID));
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
