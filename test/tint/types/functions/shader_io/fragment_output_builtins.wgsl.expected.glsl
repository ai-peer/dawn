#version 310 es
precision highp float;

struct FragDepthClampArgs {
  float min;
  float max;
};

struct frag_depth_clamp_args_block {
  FragDepthClampArgs inner;
};

layout(location=0) uniform frag_depth_clamp_args_block frag_depth_clamp_args;
float clamp_frag_depth(float v) {
  return clamp(v, frag_depth_clamp_args.inner.min, frag_depth_clamp_args.inner.max);
}

float main1() {
  return clamp_frag_depth(1.0f);
}

void main() {
  float inner_result = main1();
  gl_FragDepth = inner_result;
  return;
}
#version 310 es
#extension GL_OES_sample_variables : require
precision highp float;

uint main2() {
  return 1u;
}

void main() {
  uint inner_result = main2();
  gl_SampleMask[0] = int(inner_result);
  return;
}
