enable f16;

fn modf_7305a9() {
  var res = modf(f16());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  modf_7305a9();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  modf_7305a9();
}

@compute @workgroup_size(1)
fn compute_main() {
  modf_7305a9();
}
