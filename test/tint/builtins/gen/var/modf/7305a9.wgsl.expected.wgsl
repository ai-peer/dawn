enable f16;

fn modf_7305a9() {
  var arg_0 = f16();
  var res = modf(arg_0);
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
