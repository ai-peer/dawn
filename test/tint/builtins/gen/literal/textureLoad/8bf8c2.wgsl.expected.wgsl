@group(1) @binding(0) var arg_0 : texture_storage_1d<r32float, read_write>;

fn textureLoad_8bf8c2() {
  var res : vec4<f32> = textureLoad(arg_0, 1u);
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : vec4<f32>;

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureLoad_8bf8c2();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureLoad_8bf8c2();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureLoad_8bf8c2();
}