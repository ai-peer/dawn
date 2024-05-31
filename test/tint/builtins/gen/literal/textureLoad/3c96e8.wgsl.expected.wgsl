@group(1) @binding(0) var arg_0 : texture_2d_array<f32>;

fn textureLoad_3c96e8() {
  var res : vec4<f32> = textureLoad(arg_0, vec2<i32>(1i), 1u, 1u);
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : vec4<f32>;

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureLoad_3c96e8();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureLoad_3c96e8();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureLoad_3c96e8();
}