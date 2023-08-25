@group(0) @binding(0) var texture0 : texture_multisampled_2d<f32>;

@group(0) @binding(1) var texture1 : texture_depth_multisampled_2d;

struct Results {
  colorSamples : array<f32, 4>,
  depthSamples : array<f32, 4>,
}

@group(0) @binding(2) var<storage, read_write> results : Results;

@compute @workgroup_size(1)
fn main() {
  for(var i : i32 = 0; (i < 4); i = (i + 1)) {
    results.colorSamples[i] = textureLoad(texture0, vec2i(0, 0), i).x;
    results.depthSamples[i] = textureLoad(texture1, vec2i(0, 0), i);
  }
}
