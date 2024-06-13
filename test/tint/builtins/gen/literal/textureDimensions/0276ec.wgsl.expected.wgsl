@group(1) @binding(0) var arg_0 : texture_storage_3d<rgba16uint, read_write>;

fn textureDimensions_0276ec() -> vec3<u32> {
  var res : vec3<u32> = textureDimensions(arg_0);
  return res;
}

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<u32>;

@fragment
fn fragment_main() {
  prevent_dce = textureDimensions_0276ec();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = textureDimensions_0276ec();
}

struct VertexOutput {
  @builtin(position)
  pos : vec4<f32>,
  @location(0) @interpolate(flat)
  prevent_dce : vec3<u32>,
}

@vertex
fn vertex_main() -> VertexOutput {
  var out : VertexOutput;
  out.pos = vec4<f32>();
  out.prevent_dce = textureDimensions_0276ec();
  return out;
}
