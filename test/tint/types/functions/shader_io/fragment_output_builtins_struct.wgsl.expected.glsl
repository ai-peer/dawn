#version 310 es
#extension GL_OES_sample_variables : require
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

struct FragmentOutputs {
  float frag_depth;
  uint sample_mask;
};

FragmentOutputs clamp_frag_depth_FragmentOutputs(FragmentOutputs s) {
  FragmentOutputs tint_symbol_1 = FragmentOutputs(clamp_frag_depth(s.frag_depth), s.sample_mask);
  return tint_symbol_1;
}

FragmentOutputs tint_symbol() {
  FragmentOutputs tint_symbol_2 = FragmentOutputs(1.0f, 1u);
  return clamp_frag_depth_FragmentOutputs(tint_symbol_2);
}

void main() {
  FragmentOutputs inner_result = tint_symbol();
  gl_FragDepth = inner_result.frag_depth;
  gl_SampleMask[0] = int(inner_result.sample_mask);
  return;
}
