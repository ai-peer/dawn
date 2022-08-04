enable f16;

fn modf_7e44dc() {
  var res = modf(vec4<f16>(f16()));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  modf_7e44dc();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  modf_7e44dc();
}

@compute @workgroup_size(1)
fn compute_main() {
  modf_7e44dc();
}
