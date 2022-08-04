enable f16;

fn modf_8ef3ab() {
  var res = modf(vec3<f16>(f16()));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  modf_8ef3ab();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  modf_8ef3ab();
}

@compute @workgroup_size(1)
fn compute_main() {
  modf_8ef3ab();
}
