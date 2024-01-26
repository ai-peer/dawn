#version 310 es
precision highp float;

struct weights_block {
  vec2 inner;
  uint pad;
  uint pad_1;
};

layout(binding = 0, std140) uniform weights_block_ubo {
  vec2 inner;
  uint pad;
  uint pad_1;
} weights;

void tint_symbol() {
  float a = weights.inner[0];
}

void main() {
  tint_symbol();
  return;
}
