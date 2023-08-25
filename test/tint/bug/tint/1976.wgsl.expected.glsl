#version 310 es

struct Results {
  float colorSamples[4];
  float depthSamples[4];
};

layout(binding = 2, std430) buffer results_block_ssbo {
  Results inner;
} results;

uniform highp sampler2DMS texture0_1;
uniform highp sampler2DMS texture1_1;
void tint_symbol() {
  {
    for(int i = 0; (i < 4); i = (i + 1)) {
      results.inner.colorSamples[i] = texelFetch(texture0_1, ivec2(0), i).x;
      results.inner.depthSamples[i] = texelFetch(texture1_1, ivec2(0), i).x;
    }
  }
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
