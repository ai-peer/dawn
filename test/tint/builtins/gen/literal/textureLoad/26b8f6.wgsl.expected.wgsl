@group(1) @binding(0) var arg_0 : texture_storage_3d<rgba8uint, read_write>;

fn textureLoad_26b8f6() -> vec4<u32> {
  var res : vec4<u32> = textureLoad(arg_0, vec3<u32>(1u));
  return res;
}

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<u32>;

@fragment
fn fragment_main() {
  prevent_dce = textureLoad_26b8f6();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = textureLoad_26b8f6();
}

struct VertexOutput {
  @builtin(position)
  pos : vec4<f32>,
  @location(0) @interpolate(flat)
  prevent_dce : vec4<u32>,
}

@vertex
fn vertex_main() -> VertexOutput {
  var out : VertexOutput;
  out.pos = vec4<f32>();
  out.prevent_dce = textureLoad_26b8f6();
  return out;
}
